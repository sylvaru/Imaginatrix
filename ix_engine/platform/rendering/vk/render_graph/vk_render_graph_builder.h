// vk_render_graph_builder.h
#pragma once
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace ix 
{
    class RenderGraphRegistry;
    class VulkanImage;

    enum class AccessType { Read, Write };

    struct ResourceRequest {
        std::string name;
        AccessType access;
    };

    class RenderGraphBuilder 
    {
    public:
        RenderGraphBuilder(RenderGraphRegistry& registry, std::vector<ResourceRequest>& passRequests);

        void write(const std::string& name);
        void read(const std::string& name);
        void importExternalImage(const std::string& name, VulkanImage* image);

    private:
        RenderGraphRegistry& m_registry;
        std::vector<ResourceRequest>& m_currentPassRequests;
    };
}