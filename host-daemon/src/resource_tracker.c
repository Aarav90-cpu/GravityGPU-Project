/*
 * resource_tracker.c — Handle → Vulkan Object Mapping Implementation
 *
 * Simple hash table mapping guest handles to Vulkan objects.
 * Uses open addressing with linear probing.
 *
 * Copyright (c) 2026 GravityGPU Project. Educational/Research use.
 */

#include "resource_tracker.h"
#include <stdio.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════
 * Hash Function
 * ═══════════════════════════════════════════════════════════════════════ */

static inline uint32_t hash_handle(gravity_handle_t handle)
{
    /* Simple multiplicative hash (Knuth) */
    uint32_t h = handle;
    h = ((h >> 16) ^ h) * 0x45d9f3b;
    h = ((h >> 16) ^ h) * 0x45d9f3b;
    h = (h >> 16) ^ h;
    return h & (GRAVITY_TRACKER_HASH_SIZE - 1);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════ */

void gravity_resource_tracker_init(gravity_resource_tracker_t* tracker)
{
    memset(tracker, 0, sizeof(*tracker));

    /* Initialize hash table to "empty" sentinel */
    for (uint32_t i = 0; i < GRAVITY_TRACKER_HASH_SIZE; i++) {
        tracker->hash_table[i] = UINT32_MAX;
    }

    /* Initialize free list: all entries are free */
    for (uint32_t i = 0; i < GRAVITY_TRACKER_MAX_ENTRIES - 1; i++) {
        tracker->entries[i].ref_count = i + 1;  /* Next free index */
        tracker->entries[i].in_use = false;
    }
    tracker->entries[GRAVITY_TRACKER_MAX_ENTRIES - 1].ref_count = UINT32_MAX;
    tracker->entries[GRAVITY_TRACKER_MAX_ENTRIES - 1].in_use = false;
    tracker->free_head = 0;
    tracker->entry_count = 0;
}

void gravity_resource_tracker_destroy(gravity_resource_tracker_t* tracker)
{
    /* Log any leaked resources */
    if (tracker->entry_count > 0) {
        fprintf(stderr, "[resource_tracker] WARNING: %u resources leaked at shutdown!\n",
                tracker->entry_count);

        for (uint32_t i = 0; i < GRAVITY_TRACKER_MAX_ENTRIES; i++) {
            if (tracker->entries[i].in_use) {
                fprintf(stderr, "  Leaked handle 0x%08x type %d\n",
                        tracker->entries[i].handle, tracker->entries[i].type);
            }
        }
    }

    memset(tracker, 0, sizeof(*tracker));
}

gravity_resource_entry_t* gravity_resource_tracker_add(
    gravity_resource_tracker_t* tracker,
    gravity_handle_t handle,
    gravity_resource_type_t type)
{
    if (handle == GRAVITY_INVALID_HANDLE) {
        fprintf(stderr, "[resource_tracker] Cannot add INVALID_HANDLE\n");
        return NULL;
    }

    if (tracker->free_head == UINT32_MAX) {
        fprintf(stderr, "[resource_tracker] Out of resource slots!\n");
        return NULL;
    }

    /* Check for duplicate */
    if (gravity_resource_tracker_get(tracker, handle) != NULL) {
        fprintf(stderr, "[resource_tracker] Duplicate handle 0x%08x\n", handle);
        return NULL;
    }

    /* Allocate from free list */
    uint32_t idx = tracker->free_head;
    gravity_resource_entry_t* entry = &tracker->entries[idx];
    tracker->free_head = entry->ref_count;  /* ref_count stores next-free when not in use */

    /* Initialize entry */
    memset(entry, 0, sizeof(*entry));
    entry->handle = handle;
    entry->type = type;
    entry->ref_count = 1;
    entry->in_use = true;

    /* Insert into hash table */
    uint32_t hash = hash_handle(handle);
    for (uint32_t probe = 0; probe < GRAVITY_TRACKER_HASH_SIZE; probe++) {
        uint32_t slot = (hash + probe) & (GRAVITY_TRACKER_HASH_SIZE - 1);
        if (tracker->hash_table[slot] == UINT32_MAX) {
            tracker->hash_table[slot] = idx;
            break;
        }
    }

    tracker->entry_count++;
    return entry;
}

gravity_resource_entry_t* gravity_resource_tracker_get(
    const gravity_resource_tracker_t* tracker,
    gravity_handle_t handle)
{
    if (handle == GRAVITY_INVALID_HANDLE) return NULL;

    uint32_t hash = hash_handle(handle);
    for (uint32_t probe = 0; probe < GRAVITY_TRACKER_HASH_SIZE; probe++) {
        uint32_t slot = (hash + probe) & (GRAVITY_TRACKER_HASH_SIZE - 1);
        uint32_t idx = tracker->hash_table[slot];

        if (idx == UINT32_MAX) {
            return NULL;  /* Empty slot — not found */
        }

        if (tracker->entries[idx].in_use &&
            tracker->entries[idx].handle == handle) {
            return (gravity_resource_entry_t*)&tracker->entries[idx];
        }
    }

    return NULL;
}

void gravity_resource_tracker_remove(
    gravity_resource_tracker_t* tracker,
    gravity_handle_t handle)
{
    if (handle == GRAVITY_INVALID_HANDLE) return;

    uint32_t hash = hash_handle(handle);
    for (uint32_t probe = 0; probe < GRAVITY_TRACKER_HASH_SIZE; probe++) {
        uint32_t slot = (hash + probe) & (GRAVITY_TRACKER_HASH_SIZE - 1);
        uint32_t idx = tracker->hash_table[slot];

        if (idx == UINT32_MAX) {
            return;  /* Not found */
        }

        if (tracker->entries[idx].in_use &&
            tracker->entries[idx].handle == handle) {
            /* Mark entry as free */
            tracker->entries[idx].in_use = false;
            tracker->entries[idx].handle = GRAVITY_INVALID_HANDLE;

            /* Return to free list */
            tracker->entries[idx].ref_count = tracker->free_head;
            tracker->free_head = idx;

            /* Remove from hash table */
            tracker->hash_table[slot] = UINT32_MAX;

            tracker->entry_count--;

            /* TODO: Rehash displaced entries (Robin Hood deletion) */
            return;
        }
    }
}

uint32_t gravity_resource_tracker_count(const gravity_resource_tracker_t* tracker)
{
    return tracker->entry_count;
}
