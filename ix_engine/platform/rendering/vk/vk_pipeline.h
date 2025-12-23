// vk_pipeline.h
#pragma once
#include <vulkan/vulkan.h>
#include <vector>

namespace ix
{
    class VulkanContext;


    struct PipelineState {
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
        VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
        VkFrontFace frontFace = VK_FRONT_FACE_CLOCKWISE;
        VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
        VkBool32 depthTest = VK_TRUE;
        VkBool32 depthWrite = VK_TRUE;

        std::vector<VkFormat> colorAttachmentFormats;
        VkFormat depthAttachmentFormat = VK_FORMAT_UNDEFINED;

        // Explicit equality check for hash collisions
        bool operator==(const PipelineState& other) const {
            return topology == other.topology &&
                polygonMode == other.polygonMode &&
                cullMode == other.cullMode &&
                frontFace == other.frontFace &&
                depthCompareOp == other.depthCompareOp &&
                depthTest == other.depthTest &&
                depthWrite == other.depthWrite &&
                depthAttachmentFormat == other.depthAttachmentFormat &&
                colorAttachmentFormats == other.colorAttachmentFormats;
        }
    };
    class VulkanPipeline {
    public:
        VulkanPipeline(VulkanContext& context,
            const std::string& vertPath,
            const std::string& fragPath,
            const PipelineState& state,
            VkPipelineLayout layout);
        ~VulkanPipeline();

        void bind(VkCommandBuffer cmd);

        std::vector<uint32_t> readFile(const std::string& filepath);
        void createShaderModule(VulkanContext& context, const std::vector<uint32_t>& code, VkShaderModule* shaderModule);

        VkPipeline getHandle() const { return m_pipeline; }
        VkPipelineLayout getLayout() const { return m_layout; }

    private:
        VulkanContext& m_context;
        VkPipeline m_pipeline = VK_NULL_HANDLE;
        VkPipelineLayout m_layout = VK_NULL_HANDLE;
    };
}