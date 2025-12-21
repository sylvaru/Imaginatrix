// ix_interface/layer_i.h
#pragma once
namespace ix {
    class Layer_I {
    public:
        virtual ~Layer_I() = default;

        virtual void onAttach() {}
        virtual void onDetach() {}
        virtual void onUpdate(float dt) {} // Called every frame. Use for: Animations, Cameras, Visuals.
        virtual void onFixedUpdate(float fixedDt) {} // Called at fixed intervals (e.g., 0.016s). Use for: Physics, Gameplay Logic.
        virtual void onRender(float alpha) {} // 'alpha' is the interpolation factor (how far we are between physics steps).
        virtual void onImGuiRender() {}
    };
}