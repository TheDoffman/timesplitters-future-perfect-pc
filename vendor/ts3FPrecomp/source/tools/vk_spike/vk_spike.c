/**
 * vk_spike -- a throwaway Vulkan probe for the backend plan.
 *
 * Not part of the runtime and not linked by any title. It answers the
 * questions docs/technical/vulkan-backend.md needs answered before the
 * renderer interface is cut, on the machine it runs on:
 *
 *   - which Vulkan 1.3 features and formats each GPU really has
 *     (dynamic rendering, extended dynamic state, push descriptors, 4444,
 *     D24S8, BC);
 *   - which present modes exist, and which one the plan's order picks
 *     (section 4.10: MAILBOX, IMMEDIATE, FIFO_RELAXED, then FIFO);
 *   - whether every generated shader pair in a corpus builds a pipeline
 *     against the single push-descriptor set layout (section 4.2), and how
 *     long each pipeline takes -- the mid-frame hitch section 4.5 warns of;
 *   - that a swapchain, frames in flight, a dynamic-rendering clear and
 *     swapchain recreation survive the validation layers.
 *
 *   vk_spike [--device N] [--seconds S] [--pairs pairs.txt]
 *
 * pairs.txt comes from make_manifest.py. Exit code is the number of
 * validation errors plus failed pipelines, so 0 is a clean run.
 */

#define VK_USE_PLATFORM_WIN32_KHR
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vulkan/vulkan.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FRAMES_IN_FLIGHT 2
#define MAX_IMAGES       8

#define CHECK(call) do { VkResult r_ = (call); if (r_ != VK_SUCCESS) { \
    fprintf(stderr, "%s failed: %d (line %d)\n", #call, r_, __LINE__); exit(100); } } while (0)

static int g_validation_errors, g_validation_warnings;
static int g_resized;

static VKAPI_ATTR VkBool32 VKAPI_CALL on_debug(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT types,
    const VkDebugUtilsMessengerCallbackDataEXT *data, void *user)
{
    (void)types; (void)user;
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        if (g_validation_errors++ < 20)
            fprintf(stderr, "[validation error] %s\n", data->pMessage);
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        if (g_validation_warnings++ < 10)
            fprintf(stderr, "[validation warning] %s\n", data->pMessage);
    }
    return VK_FALSE;
}

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_SIZE)
        g_resized = 1;
    if (m == WM_CLOSE) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

static double now_ms(void)
{
    static LARGE_INTEGER f;
    LARGE_INTEGER t;
    if (!f.QuadPart)
        QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t);
    return t.QuadPart * 1000.0 / f.QuadPart;
}

static int has_layer(const char *name)
{
    uint32_t n = 0, i;
    VkLayerProperties *p;
    int found = 0;
    vkEnumerateInstanceLayerProperties(&n, NULL);
    p = calloc(n ? n : 1, sizeof *p);
    vkEnumerateInstanceLayerProperties(&n, p);
    for (i = 0; i < n; i++)
        if (!strcmp(p[i].layerName, name))
            found = 1;
    free(p);
    return found;
}

static int has_device_ext(VkPhysicalDevice pd, const char *name)
{
    uint32_t n = 0, i;
    VkExtensionProperties *p;
    int found = 0;
    vkEnumerateDeviceExtensionProperties(pd, NULL, &n, NULL);
    p = calloc(n ? n : 1, sizeof *p);
    vkEnumerateDeviceExtensionProperties(pd, NULL, &n, p);
    for (i = 0; i < n; i++)
        if (!strcmp(p[i].extensionName, name))
            found = 1;
    free(p);
    return found;
}

static const char *present_mode_name(VkPresentModeKHR m)
{
    switch (m) {
    case VK_PRESENT_MODE_IMMEDIATE_KHR:    return "IMMEDIATE";
    case VK_PRESENT_MODE_MAILBOX_KHR:      return "MAILBOX";
    case VK_PRESENT_MODE_FIFO_KHR:         return "FIFO";
    case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "FIFO_RELAXED";
    default:                               return "other";
    }
}

static int format_ok(VkPhysicalDevice pd, VkFormat f, VkFormatFeatureFlags need, int optimal)
{
    VkFormatProperties p;
    vkGetPhysicalDeviceFormatProperties(pd, f, &p);
    return ((optimal ? p.optimalTilingFeatures : p.linearTilingFeatures) & need) == need;
}

/* ---- the device ---------------------------------------------------------- */

typedef struct {
    VkInstance instance;
    VkDebugUtilsMessengerEXT messenger;
    VkSurfaceKHR surface;
    VkPhysicalDevice pd;
    VkDevice dev;
    uint32_t qfamily;
    VkQueue queue;
    VkFormat depth_format;
    int push_descriptor;
    PFN_vkCmdPushDescriptorSetKHR push_fn;

    VkSwapchainKHR swapchain;
    VkFormat color_format;
    VkExtent2D extent;
    uint32_t image_count;
    VkImage images[MAX_IMAGES];
    VkSemaphore render_done[MAX_IMAGES];   /* per image: a present may still hold it */
    VkPresentModeKHR present_mode;

    VkCommandPool pool;
    VkCommandBuffer cmd[FRAMES_IN_FLIGHT];
    VkFence in_flight[FRAMES_IN_FLIGHT];
    VkSemaphore acquired[FRAMES_IN_FLIGHT];
} Spike;

static VkPresentModeKHR choose_present_mode(Spike *s)
{
    static const VkPresentModeKHR order[] = {
        VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR,
        VK_PRESENT_MODE_FIFO_RELAXED_KHR, VK_PRESENT_MODE_FIFO_KHR
    };
    VkPresentModeKHR modes[16];
    uint32_t n = 16, i, k;

    vkGetPhysicalDeviceSurfacePresentModesKHR(s->pd, s->surface, &n, modes);
    printf("  present modes:");
    for (i = 0; i < n; i++)
        printf(" %s", present_mode_name(modes[i]));
    printf("\n");
    for (k = 0; k < sizeof order / sizeof order[0]; k++)
        for (i = 0; i < n; i++)
            if (modes[i] == order[k])
                return order[k];
    return VK_PRESENT_MODE_FIFO_KHR;
}

static void create_swapchain(Spike *s, VkSwapchainKHR old)
{
    VkSurfaceCapabilitiesKHR caps;
    VkSwapchainCreateInfoKHR ci = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    VkSemaphoreCreateInfo sci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    uint32_t i;

    CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s->pd, s->surface, &caps));
    s->extent = caps.currentExtent;
    if (s->extent.width == 0xFFFFFFFFu) {
        s->extent.width = 640;
        s->extent.height = 480;
    }
    ci.surface = s->surface;
    ci.minImageCount = caps.minImageCount + 1;
    if (caps.maxImageCount && ci.minImageCount > caps.maxImageCount)
        ci.minImageCount = caps.maxImageCount;
    ci.imageFormat = s->color_format;
    ci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    ci.imageExtent = s->extent;
    ci.imageArrayLayers = 1;
    /* TRANSFER_SRC as well: screen copy and F11 capture read the back buffer. */
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = s->present_mode;
    ci.clipped = VK_TRUE;
    ci.oldSwapchain = old;
    CHECK(vkCreateSwapchainKHR(s->dev, &ci, NULL, &s->swapchain));
    if (old)
        vkDestroySwapchainKHR(s->dev, old, NULL);

    for (i = 0; i < s->image_count; i++)
        vkDestroySemaphore(s->dev, s->render_done[i], NULL);
    s->image_count = MAX_IMAGES;
    CHECK(vkGetSwapchainImagesKHR(s->dev, s->swapchain, &s->image_count, s->images));
    for (i = 0; i < s->image_count; i++)
        CHECK(vkCreateSemaphore(s->dev, &sci, NULL, &s->render_done[i]));
}

static void init_device(Spike *s, HWND hwnd, HINSTANCE hinst, int want)
{
    const char *layers[] = { "VK_LAYER_KHRONOS_validation" };
    const char *iexts[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
                            VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
    VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    VkDebugUtilsMessengerCreateInfoEXT dci = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    VkWin32SurfaceCreateInfoKHR wci = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
    VkPhysicalDevice pds[8];
    uint32_t npd = 8, i, pick = 0;
    int validation = has_layer(layers[0]);

    app.pApplicationName = "vk_spike";
    app.apiVersion = VK_API_VERSION_1_3;
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = 3;
    ici.ppEnabledExtensionNames = iexts;
    ici.enabledLayerCount = validation ? 1 : 0;
    ici.ppEnabledLayerNames = layers;
    dci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    dci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    dci.pfnUserCallback = on_debug;
    ici.pNext = &dci;
    CHECK(vkCreateInstance(&ici, NULL, &s->instance));
    {
        PFN_vkCreateDebugUtilsMessengerEXT f = (PFN_vkCreateDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(s->instance, "vkCreateDebugUtilsMessengerEXT");
        if (f)
            f(s->instance, &dci, NULL, &s->messenger);
    }
    printf("validation layer: %s\n", validation ? "on" : "NOT INSTALLED (errors will go unseen)");

    wci.hinstance = hinst;
    wci.hwnd = hwnd;
    CHECK(vkCreateWin32SurfaceKHR(s->instance, &wci, NULL, &s->surface));

    CHECK(vkEnumeratePhysicalDevices(s->instance, &npd, pds));
    for (i = 0; i < npd; i++) {
        VkPhysicalDeviceProperties p;
        VkPhysicalDeviceVulkan13Features f13 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        VkPhysicalDeviceExtendedDynamicState2FeaturesEXT eds2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT };
        VkPhysicalDeviceFeatures2 f2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        VkPhysicalDevicePushDescriptorPropertiesKHR pdp = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR };
        VkPhysicalDeviceProperties2 p2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
        VkBool32 present = VK_FALSE;

        f13.pNext = &eds2;
        f2.pNext = &f13;
        vkGetPhysicalDeviceFeatures2(pds[i], &f2);
        p2.pNext = &pdp;
        vkGetPhysicalDeviceProperties2(pds[i], &p2);
        p = p2.properties;
        vkGetPhysicalDeviceSurfaceSupportKHR(pds[i], 0, s->surface, &present);
        printf("GPU %u: %s, Vulkan %u.%u.%u%s\n", i, p.deviceName,
               VK_API_VERSION_MAJOR(p.apiVersion), VK_API_VERSION_MINOR(p.apiVersion),
               VK_API_VERSION_PATCH(p.apiVersion), (int)i == want ? "  <- chosen" : "");
        printf("  dynamicRendering %d, synchronization2 %d, extendedDynamicState2 %d, "
               "push_descriptor %d (max %u), 4444 formats %d, present on queue 0 %d\n",
               f13.dynamicRendering, f13.synchronization2, eds2.extendedDynamicState2,
               has_device_ext(pds[i], VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME),
               pdp.maxPushDescriptors,
               format_ok(pds[i], VK_FORMAT_A4R4G4B4_UNORM_PACK16, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT, 1),
               present);
        printf("  D24_UNORM_S8 depth %d, D32_SFLOAT_S8 depth %d, BC1 %d, BC3 %d, "
               "R5G6B5 %d, A1R5G5B5 %d, uniform buffer offset alignment %llu\n",
               format_ok(pds[i], VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, 1),
               format_ok(pds[i], VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, 1),
               format_ok(pds[i], VK_FORMAT_BC1_RGBA_UNORM_BLOCK, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT, 1),
               format_ok(pds[i], VK_FORMAT_BC3_UNORM_BLOCK, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT, 1),
               format_ok(pds[i], VK_FORMAT_R5G6B5_UNORM_PACK16, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT, 1),
               format_ok(pds[i], VK_FORMAT_A1R5G5B5_UNORM_PACK16, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT, 1),
               (unsigned long long)p.limits.minUniformBufferOffsetAlignment);
        if ((int)i == want)
            pick = i;
    }
    if (want < 0 || want >= (int)npd) {
        /* Default: the first discrete GPU, else GPU 0. */
        for (i = 0; i < npd; i++) {
            VkPhysicalDeviceProperties p;
            vkGetPhysicalDeviceProperties(pds[i], &p);
            if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                pick = i;
                break;
            }
        }
        printf("chosen: GPU %u\n", pick);
    }
    s->pd = pds[pick];

    {
        uint32_t nq = 16;
        VkQueueFamilyProperties q[16];
        float prio = 1.0f;
        VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        VkPhysicalDeviceVulkan13Features f13 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        VkPhysicalDeviceExtendedDynamicState2FeaturesEXT eds2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT };
        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT eds1 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT };
        VkPhysicalDeviceFeatures2 f2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        VkDeviceCreateInfo dci2 = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        const char *dexts[2] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME };

        vkGetPhysicalDeviceQueueFamilyProperties(s->pd, &nq, q);
        for (i = 0; i < nq; i++) {
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(s->pd, i, s->surface, &present);
            if ((q[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present)
                break;
        }
        s->qfamily = i;
        qci.queueFamilyIndex = s->qfamily;
        qci.queueCount = 1;
        qci.pQueuePriorities = &prio;

        f13.dynamicRendering = VK_TRUE;
        f13.synchronization2 = VK_TRUE;
        /* The vertex programs write every texture coordinate as float4 and
         * some pixel shaders read float3. D3D11 links that; core Vulkan
         * refuses it unless maintenance4 (core in 1.3) relaxes the rule. */
        f13.maintenance4 = VK_TRUE;
        eds1.extendedDynamicState = VK_TRUE;
        eds2.extendedDynamicState2 = VK_TRUE;
        f13.pNext = &eds1;
        eds1.pNext = &eds2;
        f2.pNext = &f13;
        s->push_descriptor = has_device_ext(s->pd, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
        dci2.pNext = &f2;
        dci2.queueCreateInfoCount = 1;
        dci2.pQueueCreateInfos = &qci;
        dci2.enabledExtensionCount = s->push_descriptor ? 2 : 1;
        dci2.ppEnabledExtensionNames = dexts;
        CHECK(vkCreateDevice(s->pd, &dci2, NULL, &s->dev));
        vkGetDeviceQueue(s->dev, s->qfamily, 0, &s->queue);
        if (s->push_descriptor)
            s->push_fn = (PFN_vkCmdPushDescriptorSetKHR)
                vkGetDeviceProcAddr(s->dev, "vkCmdPushDescriptorSetKHR");
    }

    /* The format table must be per device (section 4.9): AMD has no D24S8. */
    s->depth_format = format_ok(s->pd, VK_FORMAT_D24_UNORM_S8_UINT,
                                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, 1)
                      ? VK_FORMAT_D24_UNORM_S8_UINT : VK_FORMAT_D32_SFLOAT_S8_UINT;
    s->color_format = VK_FORMAT_B8G8R8A8_UNORM;
    s->present_mode = choose_present_mode(s);
    printf("  present mode chosen: %s%s\n", present_mode_name(s->present_mode),
           s->present_mode == VK_PRESENT_MODE_FIFO_KHR
               ? "  -- WARNING: FIFO blocks, which throttled Burnout 2 under D3D11" : "");
    create_swapchain(s, VK_NULL_HANDLE);

    {
        VkCommandPoolCreateInfo pci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        VkCommandBufferAllocateInfo ai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        VkFenceCreateInfo fci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        VkSemaphoreCreateInfo sci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

        pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pci.queueFamilyIndex = s->qfamily;
        CHECK(vkCreateCommandPool(s->dev, &pci, NULL, &s->pool));
        ai.commandPool = s->pool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = FRAMES_IN_FLIGHT;
        CHECK(vkAllocateCommandBuffers(s->dev, &ai, s->cmd));
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        for (i = 0; i < FRAMES_IN_FLIGHT; i++) {
            CHECK(vkCreateFence(s->dev, &fci, NULL, &s->in_flight[i]));
            CHECK(vkCreateSemaphore(s->dev, &sci, NULL, &s->acquired[i]));
        }
    }
}

/* ---- the one descriptor set layout and the pipelines ---------------------- */

/* Section 4.2, as the corpus reflected it: b0-b3 and b7, t0-t3 and t8/t9,
 * s0-s3 and s9, shifted by the DXC flags b+0, t+16, s+32. */
static VkDescriptorSetLayout make_set_layout(Spike *s)
{
    static const struct { uint32_t binding; VkDescriptorType type; } B[] = {
        {  0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, {  1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
        {  2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, {  3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
        {  7, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
        { 16, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },  { 17, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },
        { 18, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },  { 19, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },
        { 24, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },  { 25, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },
        { 32, VK_DESCRIPTOR_TYPE_SAMPLER }, { 33, VK_DESCRIPTOR_TYPE_SAMPLER },
        { 34, VK_DESCRIPTOR_TYPE_SAMPLER }, { 35, VK_DESCRIPTOR_TYPE_SAMPLER },
        { 41, VK_DESCRIPTOR_TYPE_SAMPLER },
    };
    VkDescriptorSetLayoutBinding b[16];
    VkDescriptorSetLayoutCreateInfo ci = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    VkDescriptorSetLayout layout;
    uint32_t i;

    for (i = 0; i < 16; i++) {
        memset(&b[i], 0, sizeof b[i]);
        b[i].binding = B[i].binding;
        b[i].descriptorType = B[i].type;
        b[i].descriptorCount = 1;
        b[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    ci.flags = s->push_descriptor ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR : 0;
    ci.bindingCount = 16;
    ci.pBindings = b;
    CHECK(vkCreateDescriptorSetLayout(s->dev, &ci, NULL, &layout));
    return layout;
}

static VkShaderModule load_module(Spike *s, const char *path)
{
    FILE *f = fopen(path, "rb");
    long n;
    uint32_t *code;
    VkShaderModuleCreateInfo ci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    VkShaderModule m = VK_NULL_HANDLE;

    if (!f)
        return VK_NULL_HANDLE;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    code = malloc((size_t)n);
    fread(code, 1, (size_t)n, f);
    fclose(f);
    ci.codeSize = (size_t)n;
    ci.pCode = code;
    if (vkCreateShaderModule(s->dev, &ci, NULL, &m) != VK_SUCCESS)
        m = VK_NULL_HANDLE;
    free(code);
    return m;
}

static int build_pipelines(Spike *s, const char *pairs_path)
{
    FILE *f = fopen(pairs_path, "r");
    char line[4096];
    VkDescriptorSetLayout set_layout = make_set_layout(s);
    VkPipelineLayoutCreateInfo lci = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    VkPipelineLayout layout;
    int built = 0, failed = 0;
    double total = 0, worst = 0;

    lci.setLayoutCount = 1;
    lci.pSetLayouts = &set_layout;
    CHECK(vkCreatePipelineLayout(s->dev, &lci, NULL, &layout));
    if (!f) {
        printf("pipelines: no manifest at %s\n", pairs_path);
        return 0;
    }
    while (fgets(line, sizeof line, f)) {
        char vs_path[1024], ps_path[1024];
        char *p = line;
        int used = 0;
        VkVertexInputAttributeDescription attrs[16];
        uint32_t nattrs = 0, stride = 0;
        VkShaderModule vs, ps;

        if (sscanf(p, "%1023s %1023s%n", vs_path, ps_path, &used) != 2)
            continue;
        p += used;
        for (;;) {
            unsigned loc, comps;
            if (sscanf(p, " %u:%u%n", &loc, &comps, &used) != 2 || nattrs == 16)
                break;
            p += used;
            attrs[nattrs].location = loc;
            attrs[nattrs].binding = 0;
            attrs[nattrs].format = comps == 1 ? VK_FORMAT_R32_SFLOAT
                                 : comps == 2 ? VK_FORMAT_R32G32_SFLOAT
                                 : comps == 3 ? VK_FORMAT_R32G32B32_SFLOAT
                                 : VK_FORMAT_R32G32B32A32_SFLOAT;
            attrs[nattrs].offset = stride;
            stride += comps * 4;
            nattrs++;
        }
        vs = load_module(s, vs_path);
        ps = load_module(s, ps_path);
        if (!vs || !ps) {
            failed++;
            fprintf(stderr, "cannot load %s / %s\n", vs_path, ps_path);
            continue;
        }
        {
            VkPipelineShaderStageCreateInfo st[2] = {
                { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
                { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
            VkVertexInputBindingDescription vb = { 0, stride ? stride : 4, VK_VERTEX_INPUT_RATE_VERTEX };
            VkPipelineVertexInputStateCreateInfo vi = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
            VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
            VkPipelineViewportStateCreateInfo vp = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
            VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
            VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
            VkPipelineDepthStencilStateCreateInfo ds = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
            VkPipelineColorBlendAttachmentState ba = { 0 };
            VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
            /* What extended dynamic state takes out of the key (section 4.1). */
            static const VkDynamicState dyn[] = {
                VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
                VK_DYNAMIC_STATE_CULL_MODE, VK_DYNAMIC_STATE_FRONT_FACE,
                VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
                VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE, VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
                VK_DYNAMIC_STATE_DEPTH_COMPARE_OP, VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,
                VK_DYNAMIC_STATE_STENCIL_OP, VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE,
                VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE,
            };
            VkPipelineDynamicStateCreateInfo dy = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
            VkPipelineRenderingCreateInfo ri = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
            VkGraphicsPipelineCreateInfo gci = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
            VkPipeline pipe;
            double t0, dt;
            VkResult r;

            st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;   st[0].module = vs; st[0].pName = "main";
            st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; st[1].module = ps; st[1].pName = "main";
            if (strstr(vs_path, "-vs_main-")) st[0].pName = "vs_main";
            if (strstr(ps_path, "-ps_main-")) st[1].pName = "ps_main";
            vi.vertexBindingDescriptionCount = 1;
            vi.pVertexBindingDescriptions = &vb;
            vi.vertexAttributeDescriptionCount = nattrs;
            vi.pVertexAttributeDescriptions = attrs;
            ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            vp.viewportCount = 1;
            vp.scissorCount = 1;
            rs.polygonMode = VK_POLYGON_MODE_FILL;
            rs.lineWidth = 1.0f;
            ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            ba.colorWriteMask = 0xF;
            cb.attachmentCount = 1;
            cb.pAttachments = &ba;
            dy.dynamicStateCount = sizeof dyn / sizeof dyn[0];
            dy.pDynamicStates = dyn;
            ri.colorAttachmentCount = 1;
            ri.pColorAttachmentFormats = &s->color_format;
            ri.depthAttachmentFormat = s->depth_format;
            ri.stencilAttachmentFormat = s->depth_format;
            gci.pNext = &ri;
            gci.stageCount = 2;
            gci.pStages = st;
            gci.pVertexInputState = &vi;
            gci.pInputAssemblyState = &ia;
            gci.pViewportState = &vp;
            gci.pRasterizationState = &rs;
            gci.pMultisampleState = &ms;
            gci.pDepthStencilState = &ds;
            gci.pColorBlendState = &cb;
            gci.pDynamicState = &dy;
            gci.layout = layout;

            t0 = now_ms();
            r = vkCreateGraphicsPipelines(s->dev, VK_NULL_HANDLE, 1, &gci, NULL, &pipe);
            dt = now_ms() - t0;
            if (r == VK_SUCCESS) {
                built++;
                total += dt;
                if (dt > worst)
                    worst = dt;
                vkDestroyPipeline(s->dev, pipe, NULL);
            } else {
                failed++;
                fprintf(stderr, "pipeline failed (%d): %s + %s\n", r, vs_path, ps_path);
            }
        }
        vkDestroyShaderModule(s->dev, vs, NULL);
        vkDestroyShaderModule(s->dev, ps, NULL);
    }
    fclose(f);
    printf("pipelines: %d built, %d failed; %.2f ms mean, %.2f ms worst (no pipeline cache)\n",
           built, failed, built ? total / built : 0.0, worst);
    vkDestroyPipelineLayout(s->dev, layout, NULL);
    vkDestroyDescriptorSetLayout(s->dev, set_layout, NULL);
    return failed;
}

/* ---- the frame loop --------------------------------------------------------- */

static void image_barrier(VkCommandBuffer cb, VkImage img, VkImageLayout from, VkImageLayout to,
                          VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access,
                          VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access)
{
    VkImageMemoryBarrier2 b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    VkDependencyInfo d = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

    b.srcStageMask = src_stage;
    b.srcAccessMask = src_access;
    b.dstStageMask = dst_stage;
    b.dstAccessMask = dst_access;
    b.oldLayout = from;
    b.newLayout = to;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.layerCount = 1;
    d.imageMemoryBarrierCount = 1;
    d.pImageMemoryBarriers = &b;
    vkCmdPipelineBarrier2(cb, &d);
}

static int run_frames(Spike *s, double seconds)
{
    double start = now_ms(), last_report = start;
    uint32_t frame = 0, frames = 0, recreated = 0;
    MSG msg;

    while (now_ms() - start < seconds * 1000.0) {
        uint32_t fi = frame % FRAMES_IN_FLIGHT, img;
        VkCommandBuffer cb = s->cmd[fi];
        VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        VkRenderingAttachmentInfo ca = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        VkRenderingInfo ri = { VK_STRUCTURE_TYPE_RENDERING_INFO };
        VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        VkPresentInfoKHR pi = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        VkResult r;
        float t = (float)((now_ms() - start) / 1000.0);

        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                return 0;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (g_resized) {
            VkSurfaceCapabilitiesKHR caps;
            g_resized = 0;
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s->pd, s->surface, &caps);
            if (caps.currentExtent.width == 0)   /* minimised: nothing to draw into */
                continue;
            vkDeviceWaitIdle(s->dev);
            create_swapchain(s, s->swapchain);
            recreated++;
        }

        CHECK(vkWaitForFences(s->dev, 1, &s->in_flight[fi], VK_TRUE, UINT64_MAX));
        r = vkAcquireNextImageKHR(s->dev, s->swapchain, UINT64_MAX, s->acquired[fi],
                                  VK_NULL_HANDLE, &img);
        if (r == VK_ERROR_OUT_OF_DATE_KHR) {
            g_resized = 1;
            continue;
        }
        CHECK(vkResetFences(s->dev, 1, &s->in_flight[fi]));
        CHECK(vkResetCommandBuffer(cb, 0));
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        CHECK(vkBeginCommandBuffer(cb, &bi));
        image_barrier(cb, s->images[img], VK_IMAGE_LAYOUT_UNDEFINED,
                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
        {
            VkImageViewCreateInfo vci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            VkImageView view;
            vci.image = s->images[img];
            vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            vci.format = s->color_format;
            vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            vci.subresourceRange.levelCount = 1;
            vci.subresourceRange.layerCount = 1;
            CHECK(vkCreateImageView(s->dev, &vci, NULL, &view));
            ca.imageView = view;
            ca.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            /* A full-target clear is a load op, the fast path (section 4.8). */
            ca.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            ca.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            ca.clearValue.color.float32[0] = 0.10f;
            ca.clearValue.color.float32[1] = 0.15f + 0.1f * (float)(t - (int)t);
            ca.clearValue.color.float32[2] = 0.40f;
            ca.clearValue.color.float32[3] = 1.0f;
            ri.renderArea.extent = s->extent;
            ri.layerCount = 1;
            ri.colorAttachmentCount = 1;
            ri.pColorAttachments = &ca;
            vkCmdBeginRendering(cb, &ri);
            {
                /* The Y-flip (section 4.4): a negative height, set here once. */
                VkViewport vp = { 0, (float)s->extent.height, (float)s->extent.width,
                                  -(float)s->extent.height, 0.0f, 1.0f };
                vkCmdSetViewport(cb, 0, 1, &vp);
            }
            vkCmdEndRendering(cb);
            image_barrier(cb, s->images[img], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                          VK_PIPELINE_STAGE_2_NONE, 0);
            CHECK(vkEndCommandBuffer(cb));

            si.waitSemaphoreCount = 1;
            si.pWaitSemaphores = &s->acquired[fi];
            si.pWaitDstStageMask = &wait;
            si.commandBufferCount = 1;
            si.pCommandBuffers = &cb;
            si.signalSemaphoreCount = 1;
            si.pSignalSemaphores = &s->render_done[img];
            CHECK(vkQueueSubmit(s->queue, 1, &si, s->in_flight[fi]));
            pi.waitSemaphoreCount = 1;
            pi.pWaitSemaphores = &s->render_done[img];
            pi.swapchainCount = 1;
            pi.pSwapchains = &s->swapchain;
            pi.pImageIndices = &img;
            r = vkQueuePresentKHR(s->queue, &pi);
            if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR)
                g_resized = 1;
            /* Views are per frame here for brevity; the fence above makes the
             * destroy safe only after the queue is idle, so wait for it. */
            vkQueueWaitIdle(s->queue);
            vkDestroyImageView(s->dev, view, NULL);
        }
        frame++;
        frames++;
        if (now_ms() - last_report >= 1000.0) {
            printf("  %u frames in the last second\n", frames);
            frames = 0;
            last_report = now_ms();
        }
    }
    vkDeviceWaitIdle(s->dev);
    printf("frames: %u presented, swapchain recreated %u time(s)\n", frame, recreated);
    return 0;
}

int main(int argc, char **argv)
{
    Spike s;
    int want = -1, i, failed;
    double seconds = 5.0;
    const char *pairs = NULL;
    HINSTANCE hinst = GetModuleHandleA(NULL);
    WNDCLASSA wc = { 0 };
    HWND hwnd;

    for (i = 1; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--device"))  want = atoi(argv[i + 1]);
        if (!strcmp(argv[i], "--seconds")) seconds = atof(argv[i + 1]);
        if (!strcmp(argv[i], "--pairs"))   pairs = argv[i + 1];
    }
    memset(&s, 0, sizeof s);
    wc.lpfnWndProc = wndproc;
    wc.hInstance = hinst;
    wc.lpszClassName = "vk_spike";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);
    hwnd = CreateWindowA("vk_spike", "vk_spike", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                         CW_USEDEFAULT, CW_USEDEFAULT, 656, 519, NULL, NULL, hinst, NULL);

    init_device(&s, hwnd, hinst, want);
    failed = pairs ? build_pipelines(&s, pairs) : 0;
    run_frames(&s, seconds);

    printf("validation: %d error(s), %d warning(s)\n", g_validation_errors, g_validation_warnings);
    return g_validation_errors + failed;
}
