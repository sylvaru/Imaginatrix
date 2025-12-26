// vk_compute_pipeline.cpp
#include "common/engine_pch.h"
#include "vk_compute_pipeline.h"
#include "vk_context.h"


namespace ix
{
    VulkanComputePipeline::VulkanComputePipeline(
        VulkanContext& context,
        const std::string& computePath,
        VkPipelineLayout layout)
        : m_context(context)
        , m_layout(layout)
    {
        auto compCode = readFile(computePath);

        VkShaderModule compModule;
        createShaderModule(compCode, &compModule);

        VkPipelineShaderStageCreateInfo stageInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageInfo.module = compModule;
        stageInfo.pName = "main";

        VkComputePipelineCreateInfo pipelineCI{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
        pipelineCI.stage = stageInfo;
        pipelineCI.layout = m_layout;

        if (vkCreateComputePipelines(m_context.device(), VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &m_pipeline) != VK_SUCCESS) 
        {
            vkDestroyShaderModule(m_context.device(), compModule, nullptr);
            throw std::runtime_error("VulkanComputePipeline: Failed to create compute pipeline from " + computePath);
        }

        vkDestroyShaderModule(m_context.device(), compModule, nullptr);
    }

    VulkanComputePipeline::~VulkanComputePipeline()
    {
        if (m_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(m_context.device(), m_pipeline, nullptr);
        }
    }

    void VulkanComputePipeline::bind(VkCommandBuffer cmd) const
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    }


    std::vector<uint32_t> VulkanComputePipeline::readFile(const std::string& filepath) 
    {
        std::ifstream file(filepath, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("VulkanComputePipeline: Failed to open shader file: " + filepath);
        }

        size_t fileSize = (size_t)file.tellg();
        std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
        file.close();

        return buffer;
    }

    void VulkanComputePipeline::createShaderModule(const std::vector<uint32_t>& code, VkShaderModule* shaderModule) 
    {
        VkShaderModuleCreateInfo createInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        createInfo.codeSize = code.size() * sizeof(uint32_t);
        createInfo.pCode = code.data();

        if (vkCreateShaderModule(m_context.device(), &createInfo, nullptr, shaderModule) != VK_SUCCESS) 
        {
            throw std::runtime_error("VulkanComputePipeline: Failed to create shader module!");
        }
    }
}