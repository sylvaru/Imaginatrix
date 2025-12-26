#pragma once
#include "layer_i.h"


class GameLayer : public ix::Layer_I
{
public:
	void onAttach() override;
	void onUpdate(float dt) override;
	void onFixedUpdate(float fixedDt) override;
	void onRender(float alpha) override;

private:
};