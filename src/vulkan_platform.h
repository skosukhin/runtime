#ifndef ANYDSL_RUNTIME_VULKAN_PLATFORM_H
#define ANYDSL_RUNTIME_VULKAN_PLATFORM_H

#include "platform.h"
#include <vulkan/vulkan.h>

class VulkanPlatform : public Platform {
public:
    VulkanPlatform(Runtime* runtime);
    ~VulkanPlatform() override;

protected:
    void *alloc(DeviceId dev, int64_t size) override;
    void *alloc_host(DeviceId dev, int64_t size) override;
    void *alloc_unified(DeviceId dev, int64_t size) override { command_unavailable("alloc_unified"); }

    void *get_device_ptr(DeviceId dev, void *ptr) override;

    void release(DeviceId dev, void *ptr) override;

    void release_host(DeviceId dev, void *ptr) override;

    void launch_kernel(DeviceId dev, const LaunchParams &launch_params) override;

    void synchronize(DeviceId dev) override;

    void copy(DeviceId dev_src, const void *src, int64_t offset_src, DeviceId dev_dst, void *dst, int64_t offset_dst,
              int64_t size) override;

    void copy_from_host(const void *src, int64_t offset_src, DeviceId dev_dst, void *dst, int64_t offset_dst,
                        int64_t size) override;

    void copy_to_host(DeviceId dev_src, const void *src, int64_t offset_src, void *dst, int64_t offset_dst,
                      int64_t size) override;

    size_t dev_count() const override { return physical_devices.size(); }
    std::string name() const override { return "Vulkan"; }

    struct Device {
        VulkanPlatform& platform;
        VkPhysicalDevice physical_device;
        size_t i;

        VkDevice device;

        Device(VulkanPlatform& platform, VkPhysicalDevice physical_device, size_t i);
        ~Device();
    };

    VkInstance instance;
    std::vector<VkPhysicalDevice> physical_devices;
    std::vector<std::unique_ptr<Device>> usable_devices;
};

#endif
