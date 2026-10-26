/**
 * rhi_vulkan_vma.cpp -- the one translation unit that holds the Vulkan
 * Memory Allocator's implementation (third_party/VulkanMemoryAllocator).
 * It finds its Vulkan entry points through volk at run time, like the rest
 * of the Vulkan backend: nothing links a Vulkan loader.
 */
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "volk.h"
#include "vk_mem_alloc.h"
