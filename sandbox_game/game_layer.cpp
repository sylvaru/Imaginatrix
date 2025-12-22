#include "common/game_pch.h"
#include "game_layer.h"
#include "core/scene_manager.h"
#include "core/scene.h"
#include "engine.h"
#include <spdlog/spdlog.h>

using namespace ix;
void GameLayer::onAttach()
{

    Scene& scene = SceneManager::getActiveScene();

    std::string jsonPath = std::string(PROJECT_ROOT_DIR) + "/sandbox_game/res/pipelines/default_pipelines.json";

    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        spdlog::error("GameLayer: Could not open pipeline config at {}", jsonPath);
        return;
    }

    try {
        nlohmann::json data = nlohmann::json::parse(file);
        Engine::get().getRenderer().loadPipelines(data);
    }
    catch (const std::exception& e) {
        spdlog::error("GameLayer: JSON Parse Error: {}", e.what());
    }
}

void GameLayer::onFixedUpdate(float fixedDt) 
{

}

void GameLayer::onRender(float alpha) 
{
    // Game specific UI
}
