// game_layer.cpp
#include "common/game_pch.h"
#include "game_layer.h"
#include "engine.h"
#include "core/scene_manager.h"
#include "core/components.h"


using namespace ix;

void GameLayer::onAttach()
{

    // Define the base resource path for this specific game
    std::string resPath = std::string(PROJECT_ROOT_DIR) + "/sandbox_game/res/";

    // Setup 
    SceneManager::setSceneRoot(resPath + "scenes/");
    AssetManager::get().setModelRoot(resPath + "models/");
    AssetManager::get().setTextureRoot(resPath + "textures/");

    // Load Pipelines
    std::string pipelinePath = resPath + "pipelines/default_pipelines.json";
    std::ifstream pipelineFile(pipelinePath);
    if (pipelineFile.is_open()) {
        nlohmann::json pipelineData = nlohmann::json::parse(pipelineFile);
        Engine::get().getRenderer().loadPipelines(pipelineData);
    }

    // Load the level
    SceneManager::load("level1");

    // Stress-test grid of planes
    auto& scene = SceneManager::getActiveScene();
    auto& assetManager = AssetManager::get();

    //AssetHandle meshHandle = assetManager.loadModel("sphere.gltf");
    //TextureHandle texHandle = assetManager.loadTexture("wood_floor.jpg", false);

    //for (int x = -10; x < 10; x++)
    //{
    //    for (int y = 0; y < 6; y++)
    //    {
    //        for (int z = -10; z < 10; z++)
    //        {
    //            auto entity = scene.createEntity("SphereInstance");
    //            auto& tc = entity.addComponent<TransformComponent>();

    //            tc.position =
    //            {
    //                static_cast<float>(x) * 10.0f,
    //                static_cast<float>(y) * 10.0f,
    //                static_cast<float>(z) * 10.0f
    //            };

    //            tc.scale = { 1.f, 1.f, 1.f};

    //            entity.addComponent<MeshComponent>(meshHandle, texHandle);
    //        }

    //    }
    //}




    // Stress test for forward + 

    AssetHandle lightMesh = assetManager.loadModel("sphere.gltf");

    // Scatter 150 lights to see the clustering in action
    const int lightCount = 150;

    // Seed for consistent randomness during testing
    srand(42);

    for (int i = 0; i < lightCount; ++i)
    {
        auto light = scene.createEntity("TestLight_" + std::to_string(i));
        auto& tc = light.addComponent<TransformComponent>();

        float x = ((float)(rand() % 600) / 10.0f) - 30.0f;
        float y = 1.5f;
        float z = ((float)(rand() % 600) / 10.0f) - 30.0f;

        tc.setPosition({ x, y, z });
        tc.setScale({ 0.1f, 0.1f, 0.1f });

        auto& plc = light.addComponent<PointLightComponent>();

        // Random vibrant colors
        plc.color = glm::vec3(
            (rand() % 100) / 100.0f,
            (rand() % 100) / 100.0f,
            (rand() % 100) / 100.0f
        );

        plc.intensity = 3.0f;
        plc.radius = 30.0f;

        light.addComponent<MeshComponent>(lightMesh); 
    }
}

void GameLayer::onUpdate(float dt)
{
    SceneManager::getActiveScene().update(dt, Engine::get().getInput());
}

void GameLayer::onFixedUpdate(float fixedDt) 
{
}

void GameLayer::onRender(float alpha) 
{
    // Game specific UI
}
