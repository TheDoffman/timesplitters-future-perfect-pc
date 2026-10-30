#ifndef TSFP_VULKAN_PRESENT_OUTCOMES_H
#define TSFP_VULKAN_PRESENT_OUTCOMES_H
#include <volk.h>
/* Forward real presentation to maintain semaphore ownership, then inject the
 * API result seen by the production backend. These are classification tests,
 * not real device-loss/recovery certification. */
static PFN_vkQueuePresentKHR real_queue_present;
static VkResult injected_present, injected_acquire;
static VKAPI_ATTR VkResult VKAPI_CALL queue_present_result(VkQueue queue,const VkPresentInfoKHR *info) {
    VkResult actual=real_queue_present(queue,info);
    CHECK(actual==VK_SUCCESS || actual==VK_SUBOPTIMAL_KHR);
    return injected_present;
}
static VKAPI_ATTR VkResult VKAPI_CALL acquire_result(VkDevice device,VkSwapchainKHR swap,uint64_t timeout,
    VkSemaphore sem,VkFence fence,uint32_t *index) {
    (void)device;(void)swap;(void)timeout;(void)sem;(void)fence;(void)index;
    return injected_acquire;
}
static void vulkan_present_outcomes(void) {
    real_queue_present=vkQueuePresentKHR;vkQueuePresentKHR=queue_present_result;
    injected_present=VK_SUBOPTIMAL_KHR;CHECK(rhi_present_kind(0,RHI_PRESENT_GUEST)==0);
    injected_present=VK_ERROR_OUT_OF_DATE_KHR;CHECK(rhi_present_kind(0,RHI_PRESENT_INTERPOLATED)>0);
    injected_present=VK_ERROR_DEVICE_LOST;CHECK(rhi_present(0)<0);
    vkQueuePresentKHR=real_queue_present;
    PFN_vkAcquireNextImageKHR saved=vkAcquireNextImageKHR;
    vkAcquireNextImageKHR=acquire_result;
    injected_acquire=VK_NOT_READY;CHECK(rhi_present(0)>0);
    injected_acquire=VK_ERROR_DEVICE_LOST;CHECK(rhi_present(0)<0);
    vkAcquireNextImageKHR=saved;
    CHECK(rhi_present(0)==0);
    puts("PASS: Vulkan present/acquire outcome propagation with injected API results");
}
#endif
