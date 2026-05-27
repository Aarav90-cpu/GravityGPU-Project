/*
 * shader_compiler.c — MSL to SPIR-V compiler stub
 *
 * Copyright (c) 2026 GravityGPU Project.
 */

#include "vk_device.h"

void gravity_compile_msl_to_spirv(const char* msl_source, void** spirv_out, size_t* size_out)
{
    /* Call out to external compiler or naga here */
    *spirv_out = NULL;
    *size_out = 0;
}
