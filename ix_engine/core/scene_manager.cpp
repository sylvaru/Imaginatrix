// scene_manager.cpp
#include "common/engine_pch.h"
#include "scene_manager.h"
#include "components.h"
#include "asset_manager.h"


namespace ix
{
	SceneManager* SceneManager::s_instance = nullptr;
    std::string SceneManager::s_sceneRoot = "";
	std::unique_ptr<Scene> SceneManager::s_activeScene = nullptr;

	void SceneManager::init()
	{
		s_instance = new SceneManager();
		s_instance->s_activeScene = std::make_unique<Scene>();
	}
    void SceneManager::load(const std::string& fileName)
    {
        // Build full path to the scene file
        std::string fullPath = s_sceneRoot + fileName + ".json";

        std::ifstream file(fullPath);
        if (!file.is_open()) {
            spdlog::error("SceneManager: Could not find scene file at: {}", fullPath);
            return;
        }

        nlohmann::json sceneData;
        try {
            file >> sceneData;
        }
        catch (const std::exception& e) {
            spdlog::error("SceneManager: JSON Parse Error in {}: {}", fileName, e.what());
            return;
        }

        // Prepare the new scene
        auto newScene = std::make_unique<Scene>();
        auto& assetManager = AssetManager::get();

        if (sceneData.contains("entities")) 
        {
            for (const auto& item : sceneData["entities"]) {
                std::string name = item.value("name", "Unnamed Entity");

                // Create Entity in the new scene
                Entity entity = newScene->createEntity(name);

                if (item.contains("transform")) 
                {
                    auto& tc = entity.addComponent<TransformComponent>();
                    auto pos = item["transform"]["pos"];
                    tc.position = glm::vec3(pos[0], pos[1], pos[2]);
                }

                // Implicit Asset Loading
                if (item.contains("mesh")) 
                {
                    std::string meshName = item["mesh"];

                    AssetHandle handle = assetManager.loadModel(meshName);

                    // Attach the component with the handle provided by AssetManager
                    entity.addComponent<MeshComponent>(handle);

                    spdlog::info("SceneManager: Entity '{}' initialized with mesh '{}' (Handle: {})",
                        name, meshName, handle);
                }
            }
        }

        // Atomically swap the active scene
        s_instance->s_activeScene = std::move(newScene);
        spdlog::info("SceneManager: Successfully loaded scene '{}'", fileName);
    }

    void SceneManager::shutdown()
    {
        spdlog::info("SceneManager: Shutting down and clearing active scene...");

        if (s_activeScene) {
            s_activeScene->getRegistry().clear();
            s_activeScene.reset();
        }

        if (s_instance) {
            delete s_instance;
            s_instance = nullptr;
        }
    }

	Scene::CameraMatrices SceneManager::getActiveCameraMatrices(float aspect)
	{
		if (!s_activeScene) {
			return { glm::mat4(1.0f), glm::mat4(1.0f), glm::vec3(0.0f) };
		}
		return s_activeScene->getActiveCameraMatrices(aspect);
	}
}