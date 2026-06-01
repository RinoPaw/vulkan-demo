#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>

#define VK_CHECK(x)                                      \
    do {                                                 \
        VkResult err = (x);                              \
        if (err != VK_SUCCESS) {                         \
            std::cerr << "Vulkan error: " << err << "\n";\
            return 1;                                    \
        }                                                \
    } while (0)

int main() {
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "Termux Vulkan Probe";
    app.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &app;

    VkInstance instance{};
    VK_CHECK(vkCreateInstance(&info, nullptr, &instance));

    uint32_t count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &count, nullptr));

    std::cout << "Physical device count: " << count << "\n";

    if (count == 0) {
        std::cerr << "No Vulkan device found.\n";
        vkDestroyInstance(instance, nullptr);
        return 1;
    }

    std::vector<VkPhysicalDevice> devices(count);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &count, devices.data()));

    for (uint32_t i = 0; i < count; i++) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(devices[i], &props);

        std::cout << "Device " << i << ":\n";
        std::cout << "  Name: " << props.deviceName << "\n";
        std::cout << "  API version: "
                  << VK_VERSION_MAJOR(props.apiVersion) << "."
                  << VK_VERSION_MINOR(props.apiVersion) << "."
                  << VK_VERSION_PATCH(props.apiVersion) << "\n";
        std::cout << "  Vendor ID: " << props.vendorID << "\n";
        std::cout << "  Device ID: " << props.deviceID << "\n";
        std::cout << "  Device type: " << props.deviceType << "\n";
    }

    vkDestroyInstance(instance, nullptr);
    return 0;
}
