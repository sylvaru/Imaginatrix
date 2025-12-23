// asset_manager.h
#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include "platform/rendering/vk/resource_types/vk_resource_types.h"
#include <nlohmann/json.hpp>
#include "common/handles.h"

namespace ix {
    class VulkanContext;

    class AssetManager 
    {
    public:
        static AssetManager& get() 
        {
            static AssetManager instance;
            return instance;
        }

        void init(VulkanContext* context);

        void loadAssetList(const nlohmann::json& json);
        AssetHandle loadModel(const std::string& path_or_name);
        VulkanMesh* getMesh(AssetHandle handle);

        void setModelRoot(const std::string& root) { m_modelRoot = root; }
        void clearAssetCache();
    private:
        AssetManager() = default;
        ~AssetManager() = default;

        std::unique_ptr<VulkanMesh> loadGLTF(const std::string& path);

        VulkanContext* m_context = nullptr;
        std::string m_modelRoot = "";

        std::unordered_map<std::string, AssetHandle> m_pathMap;
        std::unordered_map<AssetHandle, std::unique_ptr<VulkanMesh>> m_meshes;

        uint32_t m_nextHandle = 1;
    };
}