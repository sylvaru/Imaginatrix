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
            AssetHandle handle = m_nextAssetHandle++;

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
        for (size_t i = 0; i < posAccessor.count; ++i) {
            vertices[i].pos = { posData[i * 3], posData[i * 3 + 1], posData[i * 3 + 2] };
            vertices[i].uv = uvData ? glm::vec2(uvData[i * 2], uvData[i * 2 + 1]) : glm::vec2(0.0f);
            vertices[i].normal = glm::vec3(0.0f, 1.0f, 0.0f);
            vertices[i].color = glm::vec4(1.0f);
        }

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
            pixels = stbi_loadf(fullPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
            format = VK_FORMAT_R32G32B32A32_SFLOAT;
        }
        else {
            pixels = stbi_load(fullPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
            format = VK_FORMAT_R8G8B8A8_SRGB;
        }

        if (!pixels) {
            spdlog::error("AssetManager: Failed to load {}", fullPath);
            return 0; // Returns slot 0 (magenta)
        }

        auto image = std::make_unique<VulkanImage>(*m_context, VkExtent2D{ (uint32_t)width, (uint32_t)height }, format,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

        uint32_t pixelSize = isHDR ? 16 : 4;
        image->uploadData(pixels, width * height * pixelSize);

        stbi_image_free(pixels);

        TextureHandle handle = m_nextAssetHandle++;
        uint32_t slot = m_nextTextureSlot++;

        m_textures[handle] = std::move(image);
        m_textureToBindlessSlot[handle] = slot;
        m_pathMap[fullPath] = handle;

        // Queue for the Renderer to update the Descriptor Set
        BindlessUpdateRequest req;
        req.slot = slot;
        req.info = m_textures[handle]->getDescriptorInfo(m_defaultSampler);

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_updateQueue.push(req);
        }

        return handle;
    }

    TextureHandle AssetManager::loadTextureFromMemory(const std::string& name, void* data, uint32_t width, uint32_t height, VkFormat format)
    {
        auto image = std::make_unique<VulkanImage>(
            *m_context,
            VkExtent2D{ width, height },
            format,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
        );

        uint32_t bytesPerPixel = (format == VK_FORMAT_R32G32B32A32_SFLOAT) ? 16 : 4;
        image->uploadData(data, width * height * bytesPerPixel);

        TextureHandle handle = m_nextAssetHandle++;
        uint32_t slot = m_nextTextureSlot++;

        m_textures[handle] = std::move(image);
        m_textureToBindlessSlot[handle] = slot;
        m_pathMap[name] = handle;

        // Push to Bindless Update Queue
        BindlessUpdateRequest req;
        req.slot = slot;
        req.info = m_textures[handle]->getDescriptorInfo(m_defaultSampler);

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_updateQueue.push(req);
        }

        spdlog::debug("AssetManager: Registered '{}' to slot {}", name, slot);
        return handle;
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


    uint32_t AssetManager::getTextureBindlessIndex(TextureHandle handle)
    {
        auto it = m_textureToBindlessSlot.find(handle);
        return (it != m_textureToBindlessSlot.end()) ? it->second : 0;
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
    void AssetManager::clearAssetCache()
    {
        spdlog::info("AssetManager: Clearing asset cache...");

        // Destroy all meshes
        m_meshes.clear();

        // Destroy all textures
        m_textures.clear();

        m_pathMap.clear();
        m_textureToBindlessSlot.clear();

        // Clear the queue
        std::lock_guard<std::mutex> lock(m_queueMutex);
        while (!m_updateQueue.empty()) m_updateQueue.pop();

        // Destroy the sampler
        if (m_defaultSampler != VK_NULL_HANDLE && m_context) {
            vkDestroySampler(m_context->device(), m_defaultSampler, nullptr);
            m_defaultSampler = VK_NULL_HANDLE;
        }

        m_nextAssetHandle = 1;
        m_nextTextureSlot = 0;
        spdlog::info("AssetManager: GPU resources released.");
    }
}