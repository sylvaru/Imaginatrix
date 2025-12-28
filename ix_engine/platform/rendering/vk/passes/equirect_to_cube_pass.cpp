#include "common/engine_pch.h"
#include "equirect_to_cube_pass.h"
#include "platform/rendering/vk/render_graph/vk_render_graph_builder.h"
#include "platform/rendering/vk/render_graph/vk_render_graph_registry.h"
#include "platform/rendering/vk/vk_pipeline_manager.h"
#include "platform/rendering/vk/vk_descriptor_manager.h"
#include "platform/rendering/vk/vk_image.h"

#include "engine.h"
#include "core/asset_manager.h"
#include "core/scene_manager.h"
#include "core/components.h"
#include "global_common/ix_global_pods.h"


namespace ix 
{

    EquirectToCubemapPass::EquirectToCubemapPass(const std::string& name) : RenderGraphPass_I(name) {}

    void EquirectToCubemapPass::setup(RenderGraphBuilder& builder) {}

    void EquirectToCubemapPass::execute(const RenderState& state, RenderGraphRegistry& registry)
    {
        VkCommandBuffer cmd = state.frame.commandBuffer;
        auto& scene = SceneManager::getActiveScene();
        TextureHandle skyHandle = scene.getSkybox();

        if (skyHandle == 0) return;

        static std::set<TextureHandle> convertedTextures;
        if (convertedTextures.count(skyHandle)) return;

        auto& assetManager = AssetManager::get();
        VulkanImage* cubemap = assetManager.getTexture(skyHandle);

        uint32_t sourceHdrIndex = assetManager.getHDRSourceBindlessIndex(skyHandle);

        if (!cubemap) return;

        // Allocate transient descriptor set for the Storage Image
        VkDescriptorSet storageSet;
        if (!state.system.descriptorManager->allocate(&storageSet, state.system.computeStorageLayout)) 
        {
            spdlog::error("ComputePass: Failed to allocate storage descriptor set");
            return;
        }

        // Create the view for the compute shader (6 layers of the cubemap)
        VkImageView storageView = cubemap->createAdditionalView(VK_IMAGE_VIEW_TYPE_2D_ARRAY, 6);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView = storageView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        DescriptorWriter writer;
        writer.writeImage(0, &imageInfo, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        writer.updateSet(*state.system.context, storageSet);

        // Transition Cubemap to GENERAL for writing
        cubemap->transition(cmd, VK_IMAGE_LAYOUT_GENERAL);

        auto* pipeline = state.system.pipelineManager->getComputePipeline("EquirectToCube");

        if (pipeline) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->getHandle());

            // Set 0: Global, Set 1: Bindless, Set 2: Storage (Output)
            VkDescriptorSet sets[] = {
                 state.frame.globalDescriptorSet,
                 state.frame.bindlessDescriptorSet,
                 storageSet
            };
            vkCmdBindDescriptorSets(state.frame.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                pipeline->getLayout(), 0, 3, sets, 0, nullptr);

            uint32_t stages = VK_SHADER_STAGE_COMPUTE_BIT;
            vkCmdPushConstants(state.frame.commandBuffer, pipeline->getLayout(),
                stages, 64, sizeof(uint32_t), &sourceHdrIndex);

            vkCmdDispatch(cmd, 512 / 16, 512 / 16, 6);
        }

        VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.image = cubemap->getHandle();
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };

        VkDependencyInfo depInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(cmd, &depInfo);

        convertedTextures.insert(skyHandle);

    }

}