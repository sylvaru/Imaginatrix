// vk_render_graph_registry.cpp
#include "common/engine_pch.h"
#include "vk_render_graph_registry.h"
#include "platform/rendering/vk/vk_image.h"

namespace ix
{
    void RenderGraphRegistry::registerExternalImage(const std::string& name, VulkanImage* image)
    {
        auto& state = m_resources[name];

        state.physicalImage = image;
        if (image) {
     
            state.currentLayout = image->getLayout();
        }
    }

    ResourceState& RenderGraphRegistry::getResourceState(const std::string& name) 
    {
        if (m_resources.find(name) == m_resources.end()) {
            spdlog::error("RenderGraphRegistry: Resource {} does not exist!", name);
        }
        return m_resources[name];
    }


    bool RenderGraphRegistry::exists(const std::string& name) const 
    {
        return m_resources.find(name) != m_resources.end();
    }

    void RenderGraphRegistry::clearExternalResources() 
    {
        m_resources.clear();
    }
}