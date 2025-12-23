// vk_render_graph_registry.h
#pragma once
#include <unordered_map>
#include <string>
#include <vulkan/vulkan.h>

namespace ix 
{
    class VulkanImage;

    struct ResourceState 
    {
        VulkanImage* physicalImage = nullptr;
        VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkAccessFlags currentAccess = 0;
        VkPipelineStageFlags currentStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    };

    class RenderGraphRegistry 
    {
    public:
        void registerExternalImage(const std::string& name, VulkanImage* image);
        ResourceState& getResourceState(const std::string& name);
        
        bool exists(const std::string& name) const;
        void clearExternalResources();

    private:
        std::unordered_map<std::string, ResourceState> m_resources;
    };
}

