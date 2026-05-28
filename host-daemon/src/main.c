/*
 * main.c — GravityGPU Host Translation Daemon (gravityd)
 *
 * Linux daemon that:
 * 1. Opens the shared memory region from the QEMU device
 * 2. Initializes Vulkan on the host GPU (Intel UHD 770)
 * 3. Reads serialized Metal commands from the ring buffer
 * 4. Translates them to Vulkan API calls
 * 5. Renders and sends completion notifications back
 *
 * Copyright (c) 2026 GravityGPU Project. Educational/Research use.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <getopt.h>
#include <time.h>

#include "gravity_protocol.h"
#include "gravity_ring.h"
#include "gravity_formats.h"

/* Forward declarations for subsystems */
#include "vk_device.h"
#include "resource_tracker.h"
#include "cmd_processor.h"
#include "metal_to_vulkan.h"

/* ═══════════════════════════════════════════════════════════════════════
 * Globals
 * ═══════════════════════════════════════════════════════════════════════ */

static volatile sig_atomic_t g_running = 1;
static gravity_ring_t        g_ring;

/* ═══════════════════════════════════════════════════════════════════════
 * Signal Handler
 * ═══════════════════════════════════════════════════════════════════════ */

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char* shmem_path;     /* POSIX shm name (e.g., /gravity-gpu) */
    size_t      shmem_size;     /* Size of shared memory region */
    bool        verbose;        /* Verbose logging */
    bool        trace_commands; /* Log every command */
    bool        validation;     /* Enable Vulkan validation layers */
} gravity_config_t;

static void config_defaults(gravity_config_t* cfg)
{
    cfg->shmem_path     = "/gravity-gpu";
    cfg->shmem_size     = 512 * 1024 * 1024;  /* 512 MB */
    cfg->verbose        = false;
    cfg->trace_commands = false;
    cfg->validation     = false;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Shared Memory Management
 * ═══════════════════════════════════════════════════════════════════════ */

static void* open_shared_memory(const gravity_config_t* cfg)
{
    /*
     * Use shm_open() to open the same POSIX shared memory object that
     * QEMU's gravity-gpu device creates. This ensures both sides
     * are reading/writing the same physical memory region.
     */
    int fd = shm_open(cfg->shmem_path, O_RDWR, 0666);
    if (fd < 0) {
        /* If QEMU hasn't created it yet, try creating it ourselves */
        fd = shm_open(cfg->shmem_path, O_RDWR | O_CREAT, 0666);
        if (fd < 0) {
            fprintf(stderr, "[gravityd] ERROR: Cannot open shared memory '%s': %s\n",
                    cfg->shmem_path, strerror(errno));
            return NULL;
        }
        /* Ensure it is world-rw regardless of umask */
        fchmod(fd, 0666);
    }

    /* Ensure the file is the right size */
    if (ftruncate(fd, cfg->shmem_size) < 0) {
        fprintf(stderr, "[gravityd] ERROR: Cannot resize shared memory: %s\n",
                strerror(errno));
        close(fd);
        return NULL;
    }

    void* ptr = mmap(NULL, cfg->shmem_size, PROT_READ | PROT_WRITE,
                     MAP_SHARED, fd, 0);
    close(fd);  /* fd no longer needed after mmap */

    if (ptr == MAP_FAILED) {
        fprintf(stderr, "[gravityd] ERROR: Cannot mmap shared memory: %s\n",
                strerror(errno));
        return NULL;
    }

    return ptr;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Command Processing Loop
 * ═══════════════════════════════════════════════════════════════════════ */

static void process_command(gravity_ring_t* ring, const gravity_cmd_header_t* hdr,
                            const void* cmd_data, gravity_config_t* cfg,
                            gravity_vk_state_t* vk, gravity_resource_tracker_t* tracker)
{
    if (cfg->trace_commands) {
        printf("[gravityd] CMD seq=%u op=0x%04x (%s) size=%u\n",
               hdr->sequence, hdr->opcode, gravity_cmd_name(hdr->opcode),
               hdr->size);
    }

    switch (hdr->opcode) {
    case GRAVITY_CMD_HELLO: {
        const gravity_cmd_hello_t* hello = (const gravity_cmd_hello_t*)cmd_data;
        printf("[gravityd] Guest connected! Protocol v%u.%u, features=0x%08x\n",
               hello->version_major, hello->version_minor, hello->guest_features);

        /* Send hello response */
        gravity_cmd_hello_response_t resp;
        gravity_cmd_init(&resp.hdr, GRAVITY_CMD_HELLO, sizeof(resp), hdr->sequence);
        resp.magic = GRAVITY_PROTOCOL_MAGIC;
        resp.version_major = GRAVITY_PROTOCOL_VERSION_MAJOR;
        resp.version_minor = GRAVITY_PROTOCOL_VERSION_MINOR;
        resp.host_features = GRAVITY_FEAT_RING_BUFFER | GRAVITY_FEAT_SHADER_MSL |
                             GRAVITY_FEAT_COMPUTE | GRAVITY_FEAT_BLIT |
                             GRAVITY_FEAT_DISPLAY;
        resp.max_texture_size = 16384;
        resp.max_buffer_size = 256 * 1024 * 1024;
        resp.max_threads_per_group = 1024;
        memset(resp.reserved, 0, sizeof(resp.reserved));

        gravity_ring_comp_write(ring, &resp, sizeof(resp));
        break;
    }

    case GRAVITY_CMD_PING: {
        gravity_cmd_header_t pong;
        gravity_cmd_init(&pong, GRAVITY_CMD_PONG, sizeof(pong), hdr->sequence);
        gravity_ring_comp_write(ring, &pong, sizeof(pong));
        break;
    }

    case GRAVITY_CMD_CREATE_BUFFER: {
        const gravity_cmd_create_buffer_t* cmd =
            (const gravity_cmd_create_buffer_t*)cmd_data;
        gravity_cmd_process_create_buffer(vk, tracker, ring, cmd);
        break;
    }

    case GRAVITY_CMD_DESTROY_BUFFER: {
        const gravity_cmd_destroy_buffer_t* cmd =
            (const gravity_cmd_destroy_buffer_t*)cmd_data;
        gravity_resource_tracker_remove(tracker, cmd->handle);
        break;
    }

    case GRAVITY_CMD_CREATE_TEXTURE: {
        const gravity_cmd_create_texture_t* cmd =
            (const gravity_cmd_create_texture_t*)cmd_data;
        gravity_cmd_process_create_texture(vk, tracker, ring, cmd);
        break;
    }

    case GRAVITY_CMD_DESTROY_TEXTURE: {
        const gravity_cmd_destroy_texture_t* cmd =
            (const gravity_cmd_destroy_texture_t*)cmd_data;
        gravity_resource_tracker_remove(tracker, cmd->handle);
        break;
    }

    case GRAVITY_CMD_CREATE_SAMPLER: {
        const gravity_cmd_create_sampler_t* cmd =
            (const gravity_cmd_create_sampler_t*)cmd_data;
        gravity_cmd_process_create_sampler(vk, tracker, cmd);
        break;
    }

    case GRAVITY_CMD_DESTROY_SAMPLER: {
        const gravity_cmd_destroy_sampler_t* cmd =
            (const gravity_cmd_destroy_sampler_t*)cmd_data;
        gravity_resource_tracker_remove(tracker, cmd->handle);
        break;
    }

    case GRAVITY_CMD_CREATE_SHADER: {
        const gravity_cmd_create_shader_t* cmd =
            (const gravity_cmd_create_shader_t*)cmd_data;
        gravity_cmd_process_create_shader(vk, tracker, ring, cmd);
        break;
    }

    case GRAVITY_CMD_CREATE_RENDER_PIPELINE: {
        const gravity_cmd_create_render_pipeline_t* cmd =
            (const gravity_cmd_create_render_pipeline_t*)cmd_data;
        gravity_cmd_process_create_render_pipeline(vk, tracker, cmd);
        break;
    }

    case GRAVITY_CMD_CREATE_DEPTH_STENCIL: {
        const gravity_cmd_create_depth_stencil_t* cmd =
            (const gravity_cmd_create_depth_stencil_t*)cmd_data;
        gravity_cmd_process_create_depth_stencil(vk, tracker, cmd);
        break;
    }

    case GRAVITY_CMD_CMDBUF_CREATE: {
        const gravity_cmd_cmdbuf_create_t* cmd =
            (const gravity_cmd_cmdbuf_create_t*)cmd_data;
        gravity_cmd_process_cmdbuf_create(vk, tracker, cmd);
        break;
    }

    case GRAVITY_CMD_CMDBUF_COMMIT: {
        const gravity_cmd_cmdbuf_commit_t* cmd =
            (const gravity_cmd_cmdbuf_commit_t*)cmd_data;
        gravity_cmd_process_cmdbuf_commit(vk, tracker, ring, cmd);
        break;
    }

    case GRAVITY_CMD_RENDER_BEGIN:
    case GRAVITY_CMD_RENDER_END:
    case GRAVITY_CMD_RENDER_SET_PIPELINE:
    case GRAVITY_CMD_RENDER_SET_VERTEX_BUFFER:
    case GRAVITY_CMD_RENDER_SET_FRAGMENT_BUFFER:
    case GRAVITY_CMD_RENDER_SET_FRAGMENT_TEXTURE:
    case GRAVITY_CMD_RENDER_SET_FRAGMENT_SAMPLER:
    case GRAVITY_CMD_RENDER_SET_VIEWPORT:
    case GRAVITY_CMD_RENDER_SET_SCISSOR:
    case GRAVITY_CMD_RENDER_SET_CULL_MODE:
    case GRAVITY_CMD_RENDER_SET_DEPTH_STENCIL:
    case GRAVITY_CMD_RENDER_SET_DEPTH_BIAS:
    case GRAVITY_CMD_RENDER_SET_BLEND_COLOR:
    case GRAVITY_CMD_RENDER_DRAW:
    case GRAVITY_CMD_RENDER_DRAW_INDEXED:
    case GRAVITY_CMD_RENDER_DRAW_INSTANCED:
        gravity_cmd_process_render(vk, tracker, ring, hdr, cmd_data);
        break;

    case GRAVITY_CMD_COMPUTE_BEGIN:
    case GRAVITY_CMD_COMPUTE_END:
    case GRAVITY_CMD_COMPUTE_SET_PIPELINE:
    case GRAVITY_CMD_COMPUTE_SET_BUFFER:
    case GRAVITY_CMD_COMPUTE_DISPATCH:
        gravity_cmd_process_compute(vk, tracker, ring, hdr, cmd_data);
        break;

    case GRAVITY_CMD_BLIT_BEGIN:
    case GRAVITY_CMD_BLIT_END:
    case GRAVITY_CMD_BLIT_COPY_BUFFER:
    case GRAVITY_CMD_BLIT_COPY_TEXTURE:
    case GRAVITY_CMD_BLIT_COPY_BUF_TO_TEX:
    case GRAVITY_CMD_BLIT_GENERATE_MIPMAPS:
        gravity_cmd_process_blit(vk, tracker, ring, hdr, cmd_data);
        break;

    case GRAVITY_CMD_PRESENT: {
        const gravity_cmd_present_t* cmd =
            (const gravity_cmd_present_t*)cmd_data;
        gravity_cmd_process_present(vk, tracker, ring, cmd);
        break;
    }

    case GRAVITY_CMD_RESIZE_DISPLAY: {
        const gravity_cmd_resize_display_t* cmd =
            (const gravity_cmd_resize_display_t*)cmd_data;
        if (cfg->verbose) {
            printf("[gravityd] Display resize: %ux%u\n", cmd->width, cmd->height);
        }
        gravity_cmd_process_resize_display(vk, cmd);
        break;
    }

    case GRAVITY_CMD_GOODBYE:
        printf("[gravityd] Guest disconnecting gracefully.\n");
        break;

    case GRAVITY_CMD_NOP:
        break;

    default:
        if (cfg->verbose) {
            printf("[gravityd] WARNING: Unhandled command 0x%04x (%s)\n",
                   hdr->opcode, gravity_cmd_name(hdr->opcode));
        }
        break;
    }
}

static void command_loop(gravity_config_t* cfg, gravity_vk_state_t* vk,
                          gravity_resource_tracker_t* tracker)
{
    /* Command read buffer — large enough for the biggest command */
    uint8_t cmd_buf[64 * 1024];
    uint64_t total_cmds = 0;
    struct timespec sleep_ts = { 0, 100000 };  /* 100 µs poll interval */

    printf("[gravityd] Entering command processing loop...\n");

    while (g_running) {
        /* Read next command from ring buffer */
        const void* cmd = gravity_ring_cmd_peek(&g_ring, cmd_buf, sizeof(cmd_buf));

        if (!cmd) {
            /* No commands available — sleep briefly to avoid spinning.
             * In production, this would use eventfd/epoll instead. */
            nanosleep(&sleep_ts, NULL);
            continue;
        }

        const gravity_cmd_header_t* hdr = (const gravity_cmd_header_t*)cmd;

        /* Validate command */
        if (gravity_cmd_validate(hdr, sizeof(cmd_buf)) < 0) {
            fprintf(stderr, "[gravityd] ERROR: Invalid command in ring buffer!\n");
            gravity_ring_cmd_consume(&g_ring, sizeof(gravity_cmd_header_t));
            continue;
        }

        /* Process the command */
        process_command(&g_ring, hdr, cmd, cfg, vk, tracker);

        /* Consume the command from the ring */
        gravity_ring_cmd_consume(&g_ring, hdr->size);
        total_cmds++;

        /* Periodic stats */
        if (cfg->verbose && (total_cmds % 10000 == 0)) {
            printf("[gravityd] Processed %lu commands\n", (unsigned long)total_cmds);
        }
    }

    printf("[gravityd] Command loop exited after %lu commands.\n",
           (unsigned long)total_cmds);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Usage & Argument Parsing
 * ═══════════════════════════════════════════════════════════════════════ */

static void print_usage(const char* prog)
{
    printf(
        "gravityd — GravityGPU Host Translation Daemon\n"
        "\n"
        "Usage: %s [OPTIONS]\n"
        "\n"
        "Options:\n"
        "  -s, --shmem PATH       Path to shared memory file [/dev/shm/gravity-gpu]\n"
        "  -m, --memsize SIZE_MB  Shared memory size in MB [512]\n"
        "  -v, --verbose          Verbose logging\n"
        "  -t, --trace            Trace all commands\n"
        "  -d, --validation       Enable Vulkan validation layers\n"
        "  -h, --help             Show this help\n"
        "\n"
        "The daemon connects to the GravityGPU QEMU device's shared memory\n"
        "region, reads Metal GPU commands from the ring buffer, translates\n"
        "them to Vulkan, and renders on the host GPU.\n"
        "\n",
        prog);
}

static struct option long_options[] = {
    { "shmem",      required_argument, NULL, 's' },
    { "memsize",    required_argument, NULL, 'm' },
    { "verbose",    no_argument,       NULL, 'v' },
    { "trace",      no_argument,       NULL, 't' },
    { "validation", no_argument,       NULL, 'd' },
    { "help",       no_argument,       NULL, 'h' },
    { NULL, 0, NULL, 0 }
};

/* ═══════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════ */

int main(int argc, char** argv)
{
    gravity_config_t cfg;
    config_defaults(&cfg);

    int opt;
    while ((opt = getopt_long(argc, argv, "s:m:vtdh", long_options, NULL)) != -1) {
        switch (opt) {
        case 's':
            cfg.shmem_path = optarg;
            break;
        case 'm':
            cfg.shmem_size = (size_t)atol(optarg) * 1024 * 1024;
            break;
        case 'v':
            cfg.verbose = true;
            break;
        case 't':
            cfg.trace_commands = true;
            break;
        case 'd':
            cfg.validation = true;
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    /* Banner */
    printf("═══════════════════════════════════════════════════\n");
    printf("  GravityGPU Host Daemon (gravityd) v%d.%d\n",
           GRAVITY_PROTOCOL_VERSION_MAJOR, GRAVITY_PROTOCOL_VERSION_MINOR);
    printf("  Metal → Vulkan Translation Engine\n");
    printf("═══════════════════════════════════════════════════\n");
    printf("  Shared memory : %s (%zu MB)\n", cfg.shmem_path, cfg.shmem_size / (1024*1024));
    printf("  Verbose       : %s\n", cfg.verbose ? "yes" : "no");
    printf("  Trace commands: %s\n", cfg.trace_commands ? "yes" : "no");
    printf("  Validation    : %s\n", cfg.validation ? "yes" : "no");
    printf("═══════════════════════════════════════════════════\n\n");

    /* Install signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Open shared memory */
    void* shmem = open_shared_memory(&cfg);
    if (!shmem) {
        return 1;
    }
    printf("[gravityd] Shared memory mapped at %p\n", shmem);

    /* Initialize ring buffer header (we act as host) */
    if (gravity_ring_init_host(shmem, (uint32_t)cfg.shmem_size,
                                4 * 1024 * 1024,    /* 4 MB cmd ring */
                                256 * 1024) < 0) {  /* 256 KB comp ring */
        fprintf(stderr, "[gravityd] ERROR: Failed to initialize ring buffer\n");
        munmap(shmem, cfg.shmem_size);
        return 1;
    }

    /* Attach to ring buffer */
    if (gravity_ring_attach(&g_ring, shmem) < 0) {
        fprintf(stderr, "[gravityd] ERROR: Failed to attach to ring buffer\n");
        munmap(shmem, cfg.shmem_size);
        return 1;
    }
    printf("[gravityd] Ring buffer initialized:\n"
           "  Command ring : %u KB at offset %u\n"
           "  Completion   : %u KB at offset %u\n"
           "  Data region  : %u MB at offset %u\n",
           g_ring.cmd_ring_size / 1024, g_ring.header->cmd_ring_offset,
           g_ring.comp_ring_size / 1024, g_ring.header->comp_ring_offset,
           g_ring.data_region_size / (1024*1024), g_ring.header->data_region_offset);

    /* Initialize Vulkan */
    gravity_vk_state_t vk_state;
    if (gravity_vk_init(&vk_state, cfg.validation) < 0) {
        fprintf(stderr, "[gravityd] ERROR: Failed to initialize Vulkan\n");
        munmap(shmem, cfg.shmem_size);
        return 1;
    }
    printf("[gravityd] Vulkan initialized on: %s\n", vk_state.device_name);

    /* Initialize resource tracker */
    gravity_resource_tracker_t tracker;
    gravity_resource_tracker_init(&tracker);

    /* Run command processing loop */
    command_loop(&cfg, &vk_state, &tracker);

    /* Cleanup */
    printf("[gravityd] Shutting down...\n");
    gravity_resource_tracker_destroy(&tracker);
    gravity_vk_destroy(&vk_state);
    munmap(shmem, cfg.shmem_size);
    printf("[gravityd] Goodbye.\n");

    return 0;
}
