/* Asset-free Vulkan lifetime probe. All query commands finish before their
 * pools are destroyed. A timeline wait deliberately does not add queue-idle
 * serialization before every destroy: late driver callbacks must retain their
 * own objects. No guest code, shaders, surfaces, windows or game files. */
#include <vulkan/vulkan.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void json_string(const char *text) {
    putchar('"');
    for(const unsigned char *p=(const unsigned char *)text;*p;p++) {
        if(*p=='"' || *p=='\\'){putchar('\\');putchar(*p);}
        else if(*p<32 || *p>=127)printf("\\u%04x",(unsigned)*p);
        else putchar(*p);
    }
    putchar('"');
}

int main(int argc,char **argv) {
    unsigned iterations=4096;
    if(argc>2)return 2;
    if(argc==2) {
        for(const char *p=argv[1];*p;p++)if(*p<'0' || *p>'9')return 2;
        char *end=NULL;unsigned long value=strtoul(argv[1],&end,10);
        if(!argv[1][0] || !end || *end || value<1 || value>16384)return 2;
        iterations=(unsigned)value;
    }
    VkInstance instance=VK_NULL_HANDLE;VkDevice device=VK_NULL_HANDLE;
    VkCommandPool commands=VK_NULL_HANDLE;VkQueryPool query=VK_NULL_HANDLE;
    VkSemaphore timeline=VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties;memset(&properties,0,sizeof properties);
    int result=2;unsigned completed=0;
#define CHECK(call) do {VkResult code=(call);if(code!=VK_SUCCESS){fprintf(stderr,"query probe: %s returned %d at line %d\n",#call,(int)code,__LINE__);goto cleanup;}} while(0)
    uint32_t count=0;VkExtensionProperties extensions[256];
    CHECK(vkEnumerateInstanceExtensionProperties(NULL,&count,NULL));
    if(count>256)goto cleanup;
    CHECK(vkEnumerateInstanceExtensionProperties(NULL,&count,extensions));
    int portability=0;
    for(uint32_t i=0;i<count;i++)if(!strcmp(extensions[i].extensionName,"VK_KHR_portability_enumeration"))portability=1;
    const char *instance_extensions[]={"VK_KHR_portability_enumeration"};
    VkApplicationInfo application={0};application.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.pApplicationName="TSFP query lifetime probe";application.apiVersion=VK_API_VERSION_1_2;
    VkInstanceCreateInfo create_instance={0};create_instance.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_instance.pApplicationInfo=&application;
    if(portability){create_instance.flags=VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;create_instance.enabledExtensionCount=1;create_instance.ppEnabledExtensionNames=instance_extensions;}
    CHECK(vkCreateInstance(&create_instance,NULL,&instance));
    VkPhysicalDevice physical[32];count=0;
    CHECK(vkEnumeratePhysicalDevices(instance,&count,NULL));
    if(!count || count>32){result=77;goto cleanup;}
    CHECK(vkEnumeratePhysicalDevices(instance,&count,physical));
    VkPhysicalDevice chosen=VK_NULL_HANDLE;uint32_t family=0;
    for(uint32_t index=0;index<count && chosen==VK_NULL_HANDLE;index++) {
        VkPhysicalDeviceFeatures2 features={0};features.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        VkPhysicalDeviceTimelineSemaphoreFeatures supported={0};supported.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
        features.pNext=&supported;vkGetPhysicalDeviceFeatures2(physical[index],&features);
        vkGetPhysicalDeviceProperties(physical[index],&properties);
        if(!supported.timelineSemaphore || properties.apiVersion<VK_API_VERSION_1_2)continue;
        uint32_t families=0;VkQueueFamilyProperties queue_properties[64];
        vkGetPhysicalDeviceQueueFamilyProperties(physical[index],&families,NULL);
        if(families>64)continue;
        vkGetPhysicalDeviceQueueFamilyProperties(physical[index],&families,queue_properties);
        for(uint32_t i=0;i<families;i++)if(queue_properties[i].queueCount && (queue_properties[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)) {
            chosen=physical[index];family=i;break;
        }
    }
    if(chosen==VK_NULL_HANDLE){result=77;goto cleanup;}
    CHECK(vkEnumerateDeviceExtensionProperties(chosen,NULL,&count,NULL));
    if(count>256)goto cleanup;
    CHECK(vkEnumerateDeviceExtensionProperties(chosen,NULL,&count,extensions));
    int subset=0;
    for(uint32_t i=0;i<count;i++)if(!strcmp(extensions[i].extensionName,"VK_KHR_portability_subset"))subset=1;
    const char *device_extensions[]={"VK_KHR_portability_subset"};
    float priority=1.0f;
    VkDeviceQueueCreateInfo queue_info={0};queue_info.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex=family;queue_info.queueCount=1;queue_info.pQueuePriorities=&priority;
    VkPhysicalDeviceTimelineSemaphoreFeatures timeline_feature={0};timeline_feature.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
    timeline_feature.timelineSemaphore=VK_TRUE;
    VkDeviceCreateInfo create_device={0};create_device.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_device.pNext=&timeline_feature;create_device.queueCreateInfoCount=1;create_device.pQueueCreateInfos=&queue_info;
    if(subset){create_device.enabledExtensionCount=1;create_device.ppEnabledExtensionNames=device_extensions;}
    CHECK(vkCreateDevice(chosen,&create_device,NULL,&device));
    VkQueue queue;vkGetDeviceQueue(device,family,0,&queue);
    VkCommandPoolCreateInfo command_info={0};command_info.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_info.queueFamilyIndex=family;command_info.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    CHECK(vkCreateCommandPool(device,&command_info,NULL,&commands));
    VkCommandBufferAllocateInfo allocate={0};allocate.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate.commandPool=commands;allocate.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;allocate.commandBufferCount=1;
    VkCommandBuffer command;CHECK(vkAllocateCommandBuffers(device,&allocate,&command));
    VkSemaphoreTypeCreateInfo semaphore_type={0};semaphore_type.sType=VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    semaphore_type.semaphoreType=VK_SEMAPHORE_TYPE_TIMELINE;
    VkSemaphoreCreateInfo semaphore_info={0};semaphore_info.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_info.pNext=&semaphore_type;CHECK(vkCreateSemaphore(device,&semaphore_info,NULL,&timeline));
    for(unsigned i=0;i<iterations;i++) {
        VkQueryPoolCreateInfo pool_info={0};pool_info.sType=VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        pool_info.queryType=VK_QUERY_TYPE_OCCLUSION;pool_info.queryCount=64;
        CHECK(vkCreateQueryPool(device,&pool_info,NULL,&query));
        CHECK(vkResetCommandBuffer(command,0));
        VkCommandBufferBeginInfo begin={0};begin.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        CHECK(vkBeginCommandBuffer(command,&begin));
        vkCmdResetQueryPool(command,query,0,64);
        CHECK(vkEndCommandBuffer(command));
        uint64_t serial=(uint64_t)i+1;
        VkTimelineSemaphoreSubmitInfo signal={0};signal.sType=VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        signal.signalSemaphoreValueCount=1;signal.pSignalSemaphoreValues=&serial;
        VkSubmitInfo submit={0};submit.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;submit.pNext=&signal;
        submit.commandBufferCount=1;submit.pCommandBuffers=&command;submit.signalSemaphoreCount=1;submit.pSignalSemaphores=&timeline;
        CHECK(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE));
        VkSemaphoreWaitInfo wait={0};wait.sType=VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        wait.semaphoreCount=1;wait.pSemaphores=&timeline;wait.pValues=&serial;
        CHECK(vkWaitSemaphores(device,&wait,UINT64_C(5000000000)));
        vkDestroyQueryPool(device,query,NULL);query=VK_NULL_HANDLE;
        /* Ordinary allocator reuse makes an illegal late callback observable;
         * the probe only touches memory it actually owns, never freed memory. */
        void *blocks[32];
        for(unsigned j=0;j<32;j++){size_t bytes=24u+((i*31u+j*17u)%2048u);blocks[j]=malloc(bytes);if(blocks[j])memset(blocks[j],0xa5,bytes);}
        for(unsigned j=0;j<32;j++)free(blocks[j]);
        completed=i+1;
        if(!(completed%512)){fprintf(stderr,"query probe: retired=%u\n",completed);fflush(stderr);}
    }
    CHECK(vkDeviceWaitIdle(device));
    result=0;
cleanup:
    if(device) {
        (void)vkDeviceWaitIdle(device);
        if(query)vkDestroyQueryPool(device,query,NULL);
        if(timeline)vkDestroySemaphore(device,timeline,NULL);
        if(commands)vkDestroyCommandPool(device,commands,NULL);
        vkDestroyDevice(device,NULL);
    }
    if(instance)vkDestroyInstance(instance,NULL);
    if(!result) {
        printf("{\"schema\":1,\"phase\":\"query-lifetime-probe\",\"iterations\":%u,\"completed\":%u,\"query_type\":\"occlusion-reset\",\"timeline_wait\":true,\"device\":",iterations,completed);
        json_string(properties.deviceName);puts("}");
    }
    if(result==77)fprintf(stderr,"query probe: required Vulkan1.2/timeline/graphics device unavailable\n");
    return result;
}
