#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

inline void vkCheck(VkResult result, const char* expr) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(
            std::string("Vulkan error ") + std::to_string(result) + " at " + expr
        );
    }
}

#define VK_CHECK(x) vkCheck((x), #x)

inline std::vector<char> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);

    if (!file) {
        throw std::runtime_error("Failed to open file: " + path);
    }

    size_t size = static_cast<size_t>(file.tellg());
    std::vector<char> data(size);

    file.seekg(0);
    file.read(data.data(), size);

    return data;
}

inline uint32_t findGraphicsQueueFamily(VkPhysicalDevice physicalDevice) {
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, nullptr);

    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevice,
        &count,
        families.data()
    );

    for (uint32_t i = 0; i < count; i++) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            return i;
        }
    }

    throw std::runtime_error("No graphics queue family found.");
}

inline VkShaderModule createShaderModule(
    VkDevice device,
    const std::vector<char>& code
) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size();
    info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module{};
    VK_CHECK(vkCreateShaderModule(device, &info, nullptr, &module));

    return module;
}

struct VulkanBase {
    VkInstance instance{};
    VkPhysicalDevice physicalDevice{};
    VkDevice device{};
    VkQueue graphicsQueue{};
    uint32_t graphicsQueueFamily = UINT32_MAX;

    void init() {
        VkApplicationInfo app{};
        app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app.pApplicationName = "Cube Depth Pipeline Check";
        app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app.pEngineName = "No Engine";
        app.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app.apiVersion = VK_API_VERSION_1_1;

        VkInstanceCreateInfo instanceInfo{};
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceInfo.pApplicationInfo = &app;

        VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &instance));

        uint32_t deviceCount = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));

        if (deviceCount == 0) {
            throw std::runtime_error("No Vulkan physical device found.");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()));

        physicalDevice = devices[0];

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(physicalDevice, &props);

        std::cout << "Using GPU: " << props.deviceName << "\n";
        std::cout << "API version: "
                  << VK_VERSION_MAJOR(props.apiVersion) << "."
                  << VK_VERSION_MINOR(props.apiVersion) << "."
                  << VK_VERSION_PATCH(props.apiVersion) << "\n";

        graphicsQueueFamily = findGraphicsQueueFamily(physicalDevice);

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

        VK_CHECK(vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device));

        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
    }

    void cleanup() {
        if (device) {
            vkDestroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
        }

        if (instance) {
            vkDestroyInstance(instance, nullptr);
            instance = VK_NULL_HANDLE;
        }
    }
};
