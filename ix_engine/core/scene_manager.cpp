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
                    auto& tJson = item["transform"];

                    // Position
                    if (tJson.contains("pos")) {
                        auto p = tJson["pos"];
                        tc.position = { p[0], p[1], p[2] };
                    }

                    // Rotation
                    if (tJson.contains("rot")) {
                        auto r = tJson["rot"];
                        glm::vec3 eulerRadians = {
                            glm::radians((float)r[0]),
                            glm::radians((float)r[1]),
                            glm::radians((float)r[2])
                        };
                        tc.rotation = glm::quat(eulerRadians);
                    }

                    // Scale
                    if (tJson.contains("scale")) {
                        auto s = tJson["scale"];
                        tc.scale = { s[0], s[1], s[2] };
                    }
                }
                if (item.contains("camera"))
                {
                    auto& cam = entity.addComponent<CameraComponent>();
                    cam.fov = item["camera"].value("fov", 75.0f);
                    cam.primary = item["camera"].value("primary", true);
                }

                if (item.contains("controller")) 
                {
                    auto& ctrl = entity.addComponent<CameraControlComponent>();
                    ctrl.moveSpeed = item["controller"].value("speed", 5.0f);
                    ctrl.lookSensitivity = item["controller"].value("sensitivity", 0.002f);
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