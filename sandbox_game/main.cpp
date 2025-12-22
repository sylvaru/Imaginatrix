#include "engine.h"
#include "game_layer.h"

int main()
{
	ix::EngineSpecification engineSpec;
	engineSpec.windowSpec.mode = ix::WindowMode::Windowed;
	ix::Engine engine(engineSpec);
	engine.pushLayer<GameLayer>();
	engine.init();
	engine.run();
	return 0;
}