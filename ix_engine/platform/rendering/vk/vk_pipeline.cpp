// vk_pipeline.cpp
#include "common/engine_pch.h"
#include "vk_pipeline.h"
#include "vk_context.h"

namespace ix
{
    VulkanPipeline::VulkanPipeline(
        VulkanContext& context,
        const std::string& vertPath,
        const std::string& fragPath,
        const PipelineState& state,
        VkPipelineLayout layout)
        : m_context(context)
        , m_layout(layout)
    {
        auto vertCode = readFile(vertPath);
        auto fragCode = readFile(fragPath);
        VkShaderModule vertModule, fragModule;
        createShaderModule(context, vertCode, &vertModule);
        createShaderModule(context, fragCode, &fragModule);

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertModule;
        stages[0].pName = "main";

        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragModule;
        stages[1].pName = "main";

        // Dynamic Rendering Info
        VkPipelineRenderingCreateInfo renderingInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        renderingInfo.colorAttachmentCount = static_cast<uint32_t>(state.colorAttachmentFormats.size());
        renderingInfo.pColorAttachmentFormats = state.colorAttachmentFormats.data();
        renderingInfo.depthAttachmentFormat = state.depthAttachmentFormat;

        // Vertex Input
        VkPipelineVertexInputStateCreateInfo vertexInput{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

        // Input Assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        inputAssembly.topology = state.topology;

        // Dynamic States always make Viewport/Scissor dynamic
        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicInfo{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamicInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicInfo.pDynamicStates = dynamicStates.data();

        // Viewport State (Dummy values because it's dynamic)
        VkPipelineViewportStateCreateInfo viewportState{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        // Rasterizer
        VkPipelineRasterizationStateCreateInfo rasterizer{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rasterizer.polygonMode = state.polygonMode;
        rasterizer.cullMode = state.cullMode;
        rasterizer.frontFace = state.frontFace;
        rasterizer.lineWidth = 1.0f;

        // Multi-sampling
        VkPipelineMultisampleStateCreateInfo multisampling{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // Depth Stencil
        VkPipelineDepthStencilStateCreateInfo depthStencil{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        depthStencil.depthTestEnable = state.depthTest ? VK_TRUE : VK_FALSE;
        depthStencil.depthWriteEnable = state.depthWrite ? VK_TRUE : VK_FALSE;
        depthStencil.depthCompareOp = state.depthCompareOp;

        // Color Blending
        std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(state.colorAttachmentFormats.size());
        for (auto& attachment : blendAttachments) {
            attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            attachment.blendEnable = VK_FALSE;
        }

        VkPipelineColorBlendStateCreateInfo colorBlending{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlending.attachmentCount = static_cast<uint32_t>(blendAttachments.size());
        colorBlending.pAttachments = blendAttachments.data();

        // Create the pipeline
        VkGraphicsPipelineCreateInfo pipelineCI{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        pipelineCI.pNext = &renderingInfo;
        pipelineCI.stageCount = 2;
        pipelineCI.pStages = stages;
        pipelineCI.pVertexInputState = &vertexInput;
        pipelineCI.pInputAssemblyState = &inputAssembly;
        pipelineCI.pViewportState = &viewportState;
        pipelineCI.pRasterizationState = &rasterizer;
        pipelineCI.pMultisampleState = &multisampling;
        pipelineCI.pDepthStencilState = &depthStencil;
        pipelineCI.pColorBlendState = &colorBlending;
        pipelineCI.pDynamicState = &dynamicInfo;
        pipelineCI.layout = m_layout;

        if (vkCreateGraphicsPipelines(m_context.device(), VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &m_pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create modern graphics pipeline!");
        }

        // Modules can be destroyed immediately after pipeline creation
        vkDestroyShaderModule(m_context.device(), vertModule, nullptr);
        vkDestroyShaderModule(m_context.device(), fragModule, nullptr);
    }

    std::vector<uint32_t> VulkanPipeline::readFile(const std::string& filepath) {
        std::ifstream file(filepath, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file: " + filepath);
        }

        size_t fileSize = (size_t)file.tellg();
        std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
        file.close();

        return buffer;
    }

    void VulkanPipeline::createShaderModule(VulkanContext& context, const std::vector<uint32_t>& code, VkShaderModule* shaderModule) {
        VkShaderModuleCreateInfo createInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        createInfo.codeSize = code.size() * sizeof(uint32_t);
        createInfo.pCode = code.data();

        if (vkCreateShaderModule(context.device(), &createInfo, nullptr, shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }
    }

    VulkanPipeline::~VulkanPipeline() {
        vkDestroyPipeline(m_context.device(), m_pipeline, nullptr);
    }

    void VulkanPipeline::bind(VkCommandBuffer cmd) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    }
}