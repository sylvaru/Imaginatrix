#pragma once
#include "layer_i.h"

namespace ix
{
	class VulkanRenderer;

	class ImGuiLayer : public Layer_I
	{
	public:
		void onAttach() override;
		void onUpdate(float dt) override;
	private:

		VulkanRenderer* m_renderer = nullptr;
	};
}
