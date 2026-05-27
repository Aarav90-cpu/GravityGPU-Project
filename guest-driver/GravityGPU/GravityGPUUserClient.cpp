/*
 * GravityGPUUserClient.cpp — macOS Virtual GPU UserClient Implementation
 *
 * Copyright (c) 2026 GravityGPU Project. Educational/Research use.
 */

#include "GravityGPUUserClient.hpp"
#include <IOKit/IOLib.h>

#define GRAVITY_LOG(fmt, ...) IOLog("GravityGPUUserClient: " fmt "\n", ##__VA_ARGS__)
#define GRAVITY_ERR(fmt, ...) IOLog("GravityGPUUserClient ERROR: " fmt "\n", ##__VA_ARGS__)

OSDefineMetaClassAndStructors(GravityGPUUserClient, IOUserClient)

bool GravityGPUUserClient::initWithTask(task_t owningTask, void* securityToken, UInt32 type, OSDictionary* properties)
{
    if (!IOUserClient::initWithTask(owningTask, securityToken, type, properties))
        return false;
    
    fTask = owningTask;
    fProvider = nullptr;
    return true;
}

bool GravityGPUUserClient::start(IOService* provider)
{
    if (!IOUserClient::start(provider))
        return false;
    
    fProvider = OSDynamicCast(GravityGPUAccelerator, provider);
    if (!fProvider) {
        GRAVITY_ERR("Provider is not GravityGPUAccelerator");
        return false;
    }
    
    GRAVITY_LOG("UserClient started");
    return true;
}

void GravityGPUUserClient::stop(IOService* provider)
{
    GRAVITY_LOG("UserClient stopped");
    IOUserClient::stop(provider);
}

IOReturn GravityGPUUserClient::clientClose()
{
    GRAVITY_LOG("UserClient closed");
    terminate();
    return kIOReturnSuccess;
}

IOReturn GravityGPUUserClient::clientMemoryForType(UInt32 type, IOOptionBits* options, IOMemoryDescriptor** memory)
{
    /* Map shared memory structures back to userspace if needed */
    return kIOReturnUnsupported;
}

IOReturn GravityGPUUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments* arguments, IOExternalMethodDispatch* dispatch, OSObject* target, void* reference)
{
    if (selector == 0) {
        return sSubmitCommand(this, reference, arguments);
    }
    return kIOReturnBadArgument;
}

IOReturn GravityGPUUserClient::sSubmitCommand(GravityGPUUserClient* target, void* reference, IOExternalMethodArguments* arguments)
{
    if (!target->fProvider) return kIOReturnNotReady;
    
    if (arguments->structureInputSize > 0 && arguments->structureInput != nullptr) {
        int result = target->fProvider->submitCommand(arguments->structureInput, arguments->structureInputSize);
        return result == 0 ? kIOReturnSuccess : kIOReturnError;
    }
    return kIOReturnBadArgument;
}
