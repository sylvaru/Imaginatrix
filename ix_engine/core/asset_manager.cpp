#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "common/engine_pch.h"
#include "asset_manager.h"
#include "platform/rendering/vk/vk_context.h"
#include "platform/rendering/vk/vk_buffer.h"
#include <tiny_gltf.h>


namespace ix
{

    void AssetManager::init(VulkanContext* context) 
    {
        m_context = context;
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
            AssetHandle handle = m_nextHandle++;

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
            // Binary is usually less picky about paths, but we'll stick to File for it
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
        spdlog::info("AssetManager: Clearing asset cache and destroying GPU resources...");
        m_meshes.clear();
        m_pathMap.clear();
        m_nextHandle = 1;

        spdlog::info("AssetManager: GPU resources released.");
    }
}