#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declare the struct we need from the protocol */
typedef struct {
    uint16_t op;
    uint16_t size;
    uint32_t flags;
    uint64_t id;
} gravity_cmd_hdr_t;

typedef struct {
    gravity_cmd_hdr_t hdr;
    uint64_t client_id;
    uint64_t timestamp;
} gravity_cmd_ping_t;

#define GRAVITY_CMD_PING 0x0003

int main(int argc, const char * argv[]) {
    printf("[*] GravityGPU Test Application\n");

    /* 1. Find the GravityGPUAccelerator IOService */
    CFMutableDictionaryRef matchingDict = IOServiceMatching("GravityGPUAccelerator");
    if (!matchingDict) {
        printf("[-] Failed to create matching dictionary.\n");
        return 1;
    }

    io_iterator_t iter = 0;
    kern_return_t kr = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, &iter);
    if (kr != KERN_SUCCESS || iter == 0) {
        printf("[-] No GravityGPUAccelerator found. Is the kext loaded?\n");
        return 1;
    }

    io_service_t service = IOIteratorNext(iter);
    IOObjectRelease(iter);

    if (service == 0) {
        printf("[-] Iterator returned no services.\n");
        return 1;
    }
    printf("[+] Found GravityGPUAccelerator!\n");

    /* 2. Open an IOUserClient connection */
    io_connect_t connect = 0;
    kr = IOServiceOpen(service, mach_task_self(), 0, &connect);
    IOObjectRelease(service);

    if (kr != KERN_SUCCESS) {
        printf("[-] IOServiceOpen failed: 0x%08x\n", kr);
        return 1;
    }
    printf("[+] Successfully opened connection to GravityGPUUserClient!\n");

    /* 3. Prepare the command payload */
    gravity_cmd_ping_t ping;
    memset(&ping, 0, sizeof(ping));
    ping.hdr.op = GRAVITY_CMD_PING;
    ping.hdr.size = sizeof(ping);
    ping.client_id = 0xDEADBEEF;

    /* 4. Call externalMethod (selector 0 for submitCommand) */
    printf("[*] Submitting GRAVITY_CMD_PING command...\n");
    
    kr = IOConnectCallStructMethod(connect, 0, &ping, sizeof(ping), NULL, NULL);
    
    if (kr != KERN_SUCCESS) {
        printf("[-] Command submission failed: 0x%08x\n", kr);
    } else {
        printf("[+] Command submitted successfully!\n");
    }

    /* 5. Close connection */
    IOServiceClose(connect);
    printf("[*] Connection closed.\n");

    return 0;
}
