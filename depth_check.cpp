#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <stdexcept>

#define VK_CHECK(x) \
    do { \
        VkResult err = (x); \
        if (err != VK_SUCCESS) { \
            std::cerr << "Vulkan error: " << err << "\n"; \
            return 1; \
        } \
    } while (0)

int main() {
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "Depth Check";
    app.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &app;

    VkInstance instance{};
    VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &instance));

    uint32_t deviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));

    if (deviceCount == 0) {
        std::cerr << "No Vulkan device found.\n";
        return 1;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()));

    VkPhysicalDevice physicalDevice = devices[0];

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physicalDevice, &props);

    std::cout << "Using GPU: " << props.deviceName << "\n";
    std::cout << "API version: "
              << VK_VERSION_MAJOR(props.apiVersion) << "."
              << VK_VERSION_MINOR(props.apiVersion) << "."
              << VK_VERSION_PATCH(props.apiVersion) << "\n";

    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    VkFormatProperties formatProps{};
    vkGetPhysicalDeviceFormatProperties(
        physicalDevice,
        depthFormat,
        &formatProps
    );

    bool supportsDepthAttachment =
        formatProps.optimalTilingFeatures &
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

    if (!supportsDepthAttachment) {
        std::cerr << "VK_FORMAT_D32_SFLOAT is not supported as depth attachment.\n";
        vkDestroyInstance(instance, nullptr);
        return 1;
    }

    std::cout << "Depth format supported: VK_FORMAT_D32_SFLOAT\n";

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevice,
        &queueFamilyCount,
        nullptr
    );

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevice,
        &queueFamilyCount,
        queueFamilies.data()
    );

    uint32_t graphicsQueueFamily = UINT32_MAX;

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsQueueFamily = i;
            break;
        }
    }

    if (graphicsQueueFamily == UINT32_MAX) {
        std::cerr << "No graphics queue family found.\n";
        vkDestroyInstance(instance, nullptr);
        return 1;
    }

    std::cout << "Graphics queue family: " << graphicsQueueFamily << "\n";

    float priority = 1.0f;

    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = graphicsQueueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;

    VkDevice device{};
    VK_CHECK(vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device));

    VkQueue graphicsQueue{};
    vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphicsQueueFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandPool commandPool{};
    VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool));

    std::cout << "Logical device created.\n";
    std::cout << "Command pool created.\n";
    std::cout << "Depth check passed.\n";

    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    return 0;
}
