// vk_render_graph.cpp
#include "common/engine_pch.h"
#include "vk_render_graph.h"
#include "platform/rendering/vk/vk_image.h"
#include "platform/rendering/vk/vk_buffer.h"
#include "global_common/ix_global_pods.h"

namespace ix 
{
    void RenderGraph::addPass(std::unique_ptr<RenderGraphPass_I> pass) 
    
    {
        m_compiledPasses.push_back({ std::move(pass), {} });
    }

    void RenderGraph::importImage(const std::string& name, VulkanImage* image) 
    {
        m_registry.registerExternalImage(name, image);
    }

    void RenderGraph::compile(const RenderGraphCompileConfig& config)
    {
        for (auto& entry : m_compiledPasses) 
        {
            entry.requests.clear();
            PassType pType = entry.pass->getPassType();
            RenderGraphBuilder builder(m_registry, entry.requests, config, pType);
            entry.pass->setup(builder);
        }
    }

    void RenderGraph::execute(const RenderState& state) 
    {
        for (auto& entry : m_compiledPasses) 
        {
    
            for (const auto& request : entry.requests) 
            {
                transitionResource(state.frame.commandBuffer, request);
            }

            entry.pass->execute(state, m_registry);
        }
    }

    void RenderGraph::transitionResource(VkCommandBuffer cmd, const ResourceRequest& request)
    {
        auto& state = m_registry.getResourceState(request.name);

        if (state.physicalImage)
        {
            VulkanImage* img = state.physicalImage;
            VkImageLayout targetLayout;
            bool isDepth = img->isDepthFormat();

            if (isDepth) {
                if (request.passType == PassType::Graphics) {
                    // In graphics passes, depth is always an attachment
                    targetLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                }
                else {
                    // In compute/sampling, depth is a texture
                    targetLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                }
            }
            else { // Color Image
                if (request.access == AccessType::Write) {
                    targetLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                }
                else {
                    targetLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                }
            }

            if (img->getLayout() != targetLayout) {
                img->transition(cmd, targetLayout);
                state.currentLayout = targetLayout;
            }
            return;
        }

        if (state.physicalBuffer)
        {
            VkAccessFlags targetAccess = (request.access == AccessType::Write)
                ? (VK_ACCESS_SHADER_WRITE_BIT)
                : (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT);

            // Read-after-Write check
            // If the buffer was written to previously and we are now reading (or writing again)
            if (state.currentAccess & VK_ACCESS_SHADER_WRITE_BIT)
            {
                VkBufferMemoryBarrier barrier{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
                barrier.srcAccessMask = state.currentAccess;
                barrier.dstAccessMask = targetAccess;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.buffer = state.physicalBuffer->getBuffer();
                barrier.offset = 0;
                barrier.size = VK_WHOLE_SIZE;

                vkCmdPipelineBarrier(
                    cmd,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                    0, 0, nullptr, 1, &barrier, 0, nullptr
                );
            }

            state.currentAccess = targetAccess;
            return;
        }

        spdlog::error("RenderGraph: Resource '{}' has no physical image or buffer bound!", request.name);
    }

    RenderGraphPass_I* RenderGraph::getPass(const std::string& name) const
    {
        for (const auto& entry : m_compiledPasses)
        {
            if (entry.pass->getName() == name)
            {
                return entry.pass.get();
            }
        }
        return nullptr;
    }
}