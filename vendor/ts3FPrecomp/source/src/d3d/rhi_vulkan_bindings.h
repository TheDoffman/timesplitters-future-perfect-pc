/**
 * rhi_vulkan_bindings.h -- where each HLSL register lands in the Vulkan
 * backend's one descriptor set.
 *
 * HLSL has separate b, t and s register spaces and separate constant
 * buffer slots per stage; SPIR-V has one space of bindings. DXC shifts each
 * space by these amounts (rhi_vulkan_dxc.cpp), and the descriptor set layout
 * in rhi_vulkan.c declares the bindings they produce, so the two are one
 * convention and live in one place. docs/technical/vulkan-backend.md,
 * sections 4.2 and 4.3.
 *
 *   vertex b0..b7   -> bindings  0..7
 *   pixel  b0..b7   -> bindings  8..15
 *   t0..t15         -> bindings 16..31   (pixel stage)
 *   s0..s15         -> bindings 32..47   (pixel stage)
 */
#ifndef XBOXRECOMP_RHI_VULKAN_BINDINGS_H
#define XBOXRECOMP_RHI_VULKAN_BINDINGS_H

#define RHI_VK_VS_UNIFORM_BASE  0u
#define RHI_VK_PS_UNIFORM_BASE  8u
#define RHI_VK_TEXTURE_BASE    16u
#define RHI_VK_SAMPLER_BASE    32u

#endif /* XBOXRECOMP_RHI_VULKAN_BINDINGS_H */
