#include "engine.h"

#include "core/layers/imgui_layer.h"
#include "game_layer.h"

int main()
{
	ix::EngineSpecification engineSpec;
	engineSpec.windowSpec.mode = ix::WindowMode::Windowed;
	ix::Engine engine(engineSpec);
	engine.pushLayer<GameLayer>();
	engine.pushLayer<ix::ImGuiLayer>();
	engine.init();
	engine.run();
	return 0;
}