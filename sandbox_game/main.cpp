#include "engine.h"
#include "game_layer.h"

int main()
{
	ix::EngineSpecification engineSpec;
	engineSpec.windowSpec.mode = ix::WindowMode::Windowed;
	ix::Engine engine(engineSpec);
	engine.init();
	engine.pushLayer<GameLayer>();
	engine.run();
	return 0;
}