// asset_manager.h
#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <queue>
#include <mutex>
#include <nlohmann/json.hpp>
#include <functional>

#include "platform/rendering/vk/resource_types/vk_resource_types.h"
#include "common/handles.h"
#include "global_common/ix_event_pods.h"

namespace ix 
{
    class VulkanContext;
    class VulkanImage;

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
        TextureHandle loadTexture(const std::string& path, bool isHDR);
        
        VulkanMesh* getMesh(AssetHandle handle);
        VulkanImage* getTexture(TextureHandle handle);
        uint32_t getTextureBindlessIndex(TextureHandle handle);
        std::vector<BindlessUpdateRequest> takePendingUpdates();
        void setModelRoot(const std::string& root) { m_modelRoot = root; }
        void setTextureRoot(const std::string& root) { m_texRoot = root; }
        void clearAssetCache();
    private:
        AssetManager();
        ~AssetManager();

        std::unique_ptr<VulkanMesh> loadGLTF(const std::string& path);
        TextureHandle loadTextureFromMemory(const std::string& name, void* data, uint32_t width, uint32_t height, VkFormat format);

        VulkanContext* m_context = nullptr;
        std::string m_modelRoot = "";
        std::string m_texRoot = "";

        std::unordered_map<std::string, AssetHandle> m_pathMap;
        std::unordered_map<AssetHandle, std::unique_ptr<VulkanMesh>> m_meshes;

        std::unordered_map<TextureHandle, std::unique_ptr<VulkanImage>> m_textures;
        std::unordered_map<TextureHandle, uint32_t> m_textureToBindlessSlot;

        std::queue<BindlessUpdateRequest> m_updateQueue;
        std::mutex m_queueMutex;

        VkSampler m_defaultSampler = VK_NULL_HANDLE;

        uint32_t m_nextTextureSlot = 0;
        uint32_t m_nextAssetHandle = 1;
    };
}