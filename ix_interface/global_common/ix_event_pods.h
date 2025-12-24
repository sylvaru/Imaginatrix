// ix_event_pods.h
#pragma once
#include <vulkan/vulkan.h>


namespace ix
{
    struct BindlessUpdateRequest 
    {
        uint32_t slot;
        VkDescriptorImageInfo info;
    };
}