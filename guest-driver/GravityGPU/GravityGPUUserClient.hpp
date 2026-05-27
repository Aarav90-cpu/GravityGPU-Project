/*
 * GravityGPUUserClient.hpp — macOS Virtual GPU UserClient Header
 *
 * IOUserClient subclass that serves as the bridge between the
 * userspace Metal framework/WindowServer and the GravityGPUAccelerator.
 *
 * Copyright (c) 2026 GravityGPU Project. Educational/Research use.
 */

#ifndef GRAVITYGPUUSERCLIENT_HPP
#define GRAVITYGPUUSERCLIENT_HPP

#include <IOKit/IOUserClient.h>
#include "GravityGPUAccelerator.hpp"

class GravityGPUUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(GravityGPUUserClient)

public:
    virtual bool        initWithTask(task_t owningTask, void* securityToken, UInt32 type, OSDictionary* properties) override;
    virtual bool        start(IOService* provider) override;
    virtual void        stop(IOService* provider) override;
    virtual IOReturn    clientClose() override;
    virtual IOReturn    clientMemoryForType(UInt32 type, IOOptionBits* options, IOMemoryDescriptor** memory) override;
    virtual IOReturn    externalMethod(uint32_t selector, IOExternalMethodArguments* arguments, IOExternalMethodDispatch* dispatch, OSObject* target, void* reference) override;

protected:
    GravityGPUAccelerator* fProvider;
    task_t                 fTask;
    
    /* ── External Methods ── */
    static IOReturn     sSubmitCommand(GravityGPUUserClient* target, void* reference, IOExternalMethodArguments* arguments);
};

#endif /* GRAVITYGPUUSERCLIENT_HPP */
