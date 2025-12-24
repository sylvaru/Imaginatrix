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

    // Load Pipelines
    std::string pipelinePath = resPath + "pipelines/default_pipelines.json";
    std::ifstream pipelineFile(pipelinePath);
    if (pipelineFile.is_open()) {
        nlohmann::json pipelineData = nlohmann::json::parse(pipelineFile);
        Engine::get().getRenderer().loadPipelines(pipelineData);
    }

    // Load the actual level
    SceneManager::load("level1");
}

void GameLayer::onFixedUpdate(float fixedDt) 
{
    auto& input = Engine::get().getInput();
    SceneManager::getActiveScene().update(fixedDt, input);
}

void GameLayer::onRender(float alpha) 
{
    // Game specific UI
}
