#define TINYGLTF_NO_STB_IMAGE 
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "common/engine_pch.h"
#include "asset_manager.h"
#include "platform/rendering/vk/vk_context.h"
#include "platform/rendering/vk/vk_image.h"
#include "platform/rendering/vk/vk_buffer.h"

#include <tiny_gltf.h>
#include <stb_image.h>

namespace ix
{
    AssetManager::AssetManager() = default;
    AssetManager::~AssetManager() = default;

    void AssetManager::init(VulkanContext* context) 
    {
        m_context = context;

        // Default sampler for all textures
        VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 16.0f;
        vkCreateSampler(m_context->device(), &samplerInfo, nullptr, &m_defaultSampler);

        // Missing Texture (Slot 0)
        uint32_t magenta = 0xFFFF00FF;
        loadTextureFromMemory("missing_tex", &magenta, 1, 1, VK_FORMAT_R8G8B8A8_SRGB);
        spdlog::info("AssetManager: Initialized with Context at {:p}", (void*)m_context);
    }

    void AssetManager::loadAssetList(const nlohmann::json& json) 
    {
        if (json.contains("models") && json["models"].is_array()) {
            for (const auto& item : json["models"]) {
                std::string name = item["name"];
                std::string path = std::string(PROJECT_ROOT_DIR) + "/" + item.value("path", "");

                spdlog::info("AssetManager: Loading model '{}' from {}", name, path);

                AssetHandle handle = loadModel(path);

                m_pathMap[name] = handle;
            }
        }
    }

    AssetHandle AssetManager::loadModel(const std::string& name) 
    {
        // Resolve the path using the root
        std::string fullPath = m_modelRoot + name;
     

        // Prevent double loading (using the name as the key)
        if (m_pathMap.count(name)) {
            return m_pathMap[name];
        }

        spdlog::info("AssetManager: Starting load of {}", fullPath);
        auto mesh = loadGLTF(fullPath);

        if (mesh) {
            AssetHandle handle = m_nextMeshHandle++;

            m_meshRadii[handle] = mesh->boundingRadius;
            m_meshes[handle] = std::move(mesh);
            m_pathMap[name] = handle;
            return handle;
        }

        spdlog::error("AssetManager: Failed to load model: {}", fullPath);
        return 0;
    }

    std::unique_ptr<VulkanMesh> AssetManager::loadGLTF(const std::string& path)
    {
        if (!m_context) 
        { 
            spdlog::error("AssetManager::loadGLTF missing context");
            return nullptr;
        }
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err, warn;

        // Open the file manually to bypass TinyGLTF path logic
        std::ifstream stream(path, std::ios::binary);
        if (!stream) 
        {
            spdlog::error("AssetManager: System failed to open file at: {}", path);
            return nullptr;
        }

        // Get the directory containing the file
        std::filesystem::path osPath(path);
        std::string baseDir = osPath.parent_path().string();


        bool success = false;
        if (osPath.extension() == ".glb") 
        {
            success = loader.LoadBinaryFromFile(&model, &err, &warn, path);
        }
        else 
        {
            // Manual Load for ASCII
            std::ifstream ifile(path);
            if (!ifile.is_open()) {
                spdlog::error("AssetManager: Could not open file stream at {}", path);
                return nullptr;
            }

            std::stringstream buffer;
            buffer << ifile.rdbuf();
            std::string jsonStr = buffer.str();

            // Load from String
            success = loader.LoadASCIIFromString(&model, &err, &warn, jsonStr.c_str(), jsonStr.length(), baseDir);
        }

        if (!success) 
        {
            spdlog::error("AssetManager: GLTF Parse Error: {} (Warn: {})", err, warn);
            return nullptr;
        }

        // Extract Data from first mesh/primitive
        const auto& mesh = model.meshes[0];
        const auto& primitive = mesh.primitives[0];

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        // Position Data 
        if (primitive.attributes.count("POSITION") == 0) return nullptr;
        const auto& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
        const auto& posView = model.bufferViews[posAccessor.bufferView];
        const auto& posBuffer = model.buffers[posView.buffer];
        const float* posData = reinterpret_cast<const float*>(&(posBuffer.data[posView.byteOffset + posAccessor.byteOffset]));

        // UV Data 
        const float* uvData = nullptr;
        if (primitive.attributes.count("TEXCOORD_0")) {
            const auto& accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
            const auto& view = model.bufferViews[accessor.bufferView];
            const auto& buffer = model.buffers[view.buffer];
            uvData = reinterpret_cast<const float*>(&(buffer.data[view.byteOffset + accessor.byteOffset]));
        }

        // Fill Vertex Array 
        vertices.resize(posAccessor.count);
        float maxDistSq = 0.0f;

        for (size_t i = 0; i < posAccessor.count; ++i) {
            vertices[i].pos = { posData[i * 3], posData[i * 3 + 1], posData[i * 3 + 2] };

            // Track the furthest vertex from the origin (0,0,0)
            float distSq = vertices[i].pos.x * vertices[i].pos.x +
                vertices[i].pos.y * vertices[i].pos.y +
                vertices[i].pos.z * vertices[i].pos.z;
            if (distSq > maxDistSq) maxDistSq = distSq;

            vertices[i].uv = uvData ? glm::vec2(uvData[i * 2], uvData[i * 2 + 1]) : glm::vec2(0.0f);
            vertices[i].normal = glm::vec3(0.0f, 1.0f, 0.0f);
            vertices[i].color = glm::vec4(1.0f);
        }

        float meshRadius = std::sqrt(maxDistSq);

        // Index Data 
        const auto& idxAccessor = model.accessors[primitive.indices];
        const auto& idxView = model.bufferViews[idxAccessor.bufferView];
        const auto& idxBuffer = model.buffers[idxView.buffer];
        const void* rawIdx = &(idxBuffer.data[idxView.byteOffset + idxAccessor.byteOffset]);

        indices.resize(idxAccessor.count);
        if (idxAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT) 
        {
            memcpy(indices.data(), rawIdx, indices.size() * sizeof(uint32_t));
        }
        else if (idxAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT) 
        {
            const uint16_t* buf = static_cast<const uint16_t*>(rawIdx);
            for (size_t i = 0; i < idxAccessor.count; i++) indices[i] = buf[i];
        }

        // GPU Upload
        auto gpuMesh = std::make_unique<VulkanMesh>();
        gpuMesh->vertexBuffer = nullptr;
        gpuMesh->indexBuffer = nullptr;
        gpuMesh->boundingRadius = meshRadius;
        gpuMesh->vertexCount = static_cast<uint32_t>(vertices.size());
        gpuMesh->indexCount = static_cast<uint32_t>(indices.size());

        size_t vSize = vertices.size() * sizeof(Vertex);
        size_t iSize = indices.size() * sizeof(uint32_t);


        gpuMesh->vertexBuffer = std::make_unique<VulkanBuffer>(
            *m_context, vSize, 1,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY);

        gpuMesh->indexBuffer = std::make_unique<VulkanBuffer>(
            *m_context, iSize, 1,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY);

        gpuMesh->vertexBuffer->uploadData(vertices.data(), vSize);
        gpuMesh->indexBuffer->uploadData(indices.data(), iSize);

        spdlog::info("AssetManager: Loaded {} vertices and {} indices from {}", vertices.size(), indices.size(), path);

        return gpuMesh;
    }

    TextureHandle AssetManager::loadTexture(const std::string& path, bool isHDR)
    {
        if (m_pathMap.count(path)) return m_pathMap[path];

        std::string fullPath = m_texRoot + path;
        int width, height, channels;
        void* pixels = nullptr;
        VkFormat format;

        if (isHDR) {
            float* hdrPixels = stbi_loadf(fullPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
            if (!hdrPixels) {
                spdlog::error("AssetManager: stbi_loadf failed for HDR path: {}", fullPath);
                return 0;
            }

            TextureHandle handle = m_nextTextureHandle++;

            auto sourceImage = std::make_unique<VulkanImage>(
                *m_context, VkExtent2D{ (uint32_t)width, (uint32_t)height },
                VK_FORMAT_R32G32B32A32_SFLOAT,
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, 1, false
            );

            sourceImage->uploadData(hdrPixels, width * height * sizeof(float) * 4);
            stbi_image_free(hdrPixels);

            VkExtent2D cubeExtent = { 512, 512 };
            auto cubemap = std::make_unique<VulkanImage>(
                *m_context, cubeExtent,
                VK_FORMAT_R32G32B32A32_SFLOAT,
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, 6, true
            );

            uint32_t sourceSlot = m_nextTextureSlot++;
            uint32_t cubeSlot = m_nextTextureSlot++;

            m_hdrSourceBindlessSlots[handle] = sourceSlot;
            m_textureToBindlessSlot[handle] = cubeSlot;

            auto sourceInfo = sourceImage->getDescriptorInfo(m_defaultSampler);
            auto cubeInfo = cubemap->getDescriptorInfo(m_defaultSampler);

            m_hdrSources[handle] = std::move(sourceImage);
            m_textures[handle] = std::move(cubemap);
            m_pathMap[path] = handle;

            {
                std::lock_guard<std::mutex> lock(m_queueMutex);
                m_updateQueue.push({ sourceSlot, sourceInfo });
                m_updateQueue.push({ cubeSlot, cubeInfo });
            }

            spdlog::info("AssetManager: Loaded HDR '{}'. Source Index: {}, Cube Index: {}", path, sourceSlot, cubeSlot);
            return handle;
        }
        else {
            pixels = stbi_load(fullPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
            format = VK_FORMAT_R8G8B8A8_SRGB;
        }

        if (!pixels) {
            spdlog::error("AssetManager: Failed to load texture at path: {}", fullPath);
            return 0;
        }

        auto image = std::make_unique<VulkanImage>(*m_context, VkExtent2D{ (uint32_t)width, (uint32_t)height }, format,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

        image->uploadData(pixels, width * height * 4);
        stbi_image_free(pixels);

        TextureHandle handle = m_nextTextureHandle++;
        uint32_t slot = m_nextTextureSlot++;

        m_textures[handle] = std::move(image);
        m_textureToBindlessSlot[handle] = slot;
        m_pathMap[path] = handle;

        auto info = m_textures[handle]->getDescriptorInfo(m_defaultSampler);
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_updateQueue.push({ slot, info });
        }

        return handle;
    }

    uint32_t AssetManager::getTextureBindlessIndex(TextureHandle handle)
    {
        auto it = m_textureToBindlessSlot.find(handle);
        return (it != m_textureToBindlessSlot.end()) ? it->second : 0;
    }

    VulkanImage* AssetManager::getHDRSource(TextureHandle handle) 
    {
        return m_hdrSources.count(handle) ? m_hdrSources[handle].get() : nullptr;
    }

    uint32_t AssetManager::getHDRSourceBindlessIndex(TextureHandle handle) 
    {
        return m_hdrSourceBindlessSlots.count(handle) ? m_hdrSourceBindlessSlots[handle] : 0;
    }

    float AssetManager::getMeshBoundingRadius(AssetHandle handle) 
    {
        return m_meshRadii.count(handle) ? m_meshRadii[handle] : 1.0f;
    }

    VulkanImage* AssetManager::getTexture(TextureHandle handle)
    {
        auto it = m_textures.find(handle);
        if (it != m_textures.end())
        {
            return it->second.get(); 
        }

        spdlog::warn("AssetManager: Attempted to get non-existent texture handle {}", handle);
        return nullptr;
    }
    VulkanMesh* AssetManager::getMesh(AssetHandle handle) 
    {
        if (handle == 0) return nullptr;

        auto it = m_meshes.find(handle);
        if (it == m_meshes.end()) {
            spdlog::warn("AssetManager: Requested invalid handle {}", handle);
            return nullptr;
        }

        VulkanMesh* mesh = it->second.get();

        return mesh;
    }

    std::vector<BindlessUpdateRequest> AssetManager::takePendingUpdates()
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        std::vector<BindlessUpdateRequest> updates;

        while (!m_updateQueue.empty()) {
            updates.push_back(m_updateQueue.front());
            m_updateQueue.pop();
        }

        return updates;
    }


    TextureHandle AssetManager::loadTextureFromMemory(const std::string& name, void* data, uint32_t width, uint32_t height, VkFormat format)
    {
        auto image = std::make_unique<VulkanImage>(
            *m_context, VkExtent2D{ width, height }, format,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
        );

        uint32_t bytesPerPixel = (format == VK_FORMAT_R32G32B32_SFLOAT || format == VK_FORMAT_R32G32B32A32_SFLOAT) ? 16 : 4;
        image->uploadData(data, width * height * bytesPerPixel);

        TextureHandle handle = m_nextTextureHandle++;
        uint32_t slot = m_nextTextureSlot++;

        m_textures[handle] = std::move(image);
        m_textureToBindlessSlot[handle] = slot;
        m_pathMap[name] = handle;

        auto info = m_textures[handle]->getDescriptorInfo(m_defaultSampler);
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_updateQueue.push({ slot, info });
        }

        return handle;
    }

    void AssetManager::clearAssetCache()
    {
        spdlog::info("AssetManager: Clearing asset cache...");

        // Destroy all meshes
        m_meshes.clear();

        // Destroy all textures
        m_textures.clear();

        m_pathMap.clear();
        m_textureToBindlessSlot.clear();

        m_hdrSources.clear();
        m_hdrSourceBindlessSlots.clear();

        // Clear the queue
        std::lock_guard<std::mutex> lock(m_queueMutex);
        while (!m_updateQueue.empty()) m_updateQueue.pop();

        // Destroy the sampler
        if (m_defaultSampler != VK_NULL_HANDLE && m_context) {
            vkDestroySampler(m_context->device(), m_defaultSampler, nullptr);
            m_defaultSampler = VK_NULL_HANDLE;
        }

        spdlog::info("AssetManager: resources released.");
    }
}