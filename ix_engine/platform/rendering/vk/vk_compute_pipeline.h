#pragma once
#include <vulkan/vulkan.h>

namespace ix
{
    class VulkanContext;


    class VulkanComputePipeline {
    public:
        VulkanComputePipeline(VulkanContext& context, const std::string& computePath, VkPipelineLayout layout);
        ~VulkanComputePipeline();

        void bind(VkCommandBuffer cmd) const;
        VkPipeline getHandle() const { return m_pipeline; }
        VkPipelineLayout getLayout() const { return m_layout; }

        std::vector<uint32_t> readFile(const std::string& filepath);
        void createShaderModule(const std::vector<uint32_t>&code, VkShaderModule * shaderModule);
    private:
        VulkanContext& m_context;
        VkPipeline m_pipeline = VK_NULL_HANDLE;
        VkPipelineLayout m_layout = VK_NULL_HANDLE;
    };
}
