// vk_render_graph_registry.h
#pragma once
#include <unordered_map>
#include <string>
#include <vulkan/vulkan.h>

namespace ix 
{
    class VulkanImage;
    class VulkanBuffer;

    struct ResourceState 
    {
        VulkanImage* physicalImage = nullptr;
        VulkanBuffer* physicalBuffer = nullptr;
        VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkAccessFlags currentAccess = 0;
        VkPipelineStageFlags currentStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    };

    class RenderGraphRegistry 
    {
    public:
        void registerExternalImage(const std::string& name, VulkanImage* image);
        void registerExternalBuffer(const std::string& name, VulkanBuffer* buffer);

        ResourceState& getResourceState(const std::string& name);
        VulkanImage* getImage(const std::string& name);
        VulkanBuffer* getBuffer(const std::string& name);

        bool exists(const std::string& name) const;
        void clearExternalResources();

    private:
        std::unordered_map<std::string, ResourceState> m_resources;
    };
}

