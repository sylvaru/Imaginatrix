#include "common/game_pch.h"
#include "game_layer.h"
#include "core/scene_manager.h"
#include "core/scene.h"

using namespace ix;
void GameLayer::onAttach() 
{

    // Tell the Scene Manager to load the level
    //SceneManager::load("sandbox_level");
    Scene& scene = SceneManager::getActiveScene();

    //auto player = scene.createEntity("Player");
    //player.addComponent<TransformComponent>(glm::vec3(0.0f));
    //player.addComponent<CameraComponent>(true);
}

void GameLayer::onFixedUpdate(float fixedDt) 
{

}

void GameLayer::onRender(float alpha) 
{
    // Game specific UI
}
