Imaginatrix Engine

Imaginatrix is an experimental, early-stage Vulkan-powered rendering engine designed with a heavy focus on data-oriented design, modularity, and high-performance GPU-driven workflows. While not intended for production, it serves as a playground for implementing modern rendering techniques and industry-standard patterns.

Core Philosophy

    Data-Oriented Design (DOD): High-performance rendering is prioritized by separating data from logic. Utilizing POD (Plain Old Data) structures for frame state ensures cache locality and predictable data flow.

    Performance First: Minimizing CPU-side overhead through aggressive instancing, persistent memory caching, and reduced heap allocations during the render loop.

    Scalability: Designed to handle thousands of entities through efficient batching and upcoming GPU-driven culling pass.

Key Features

    Vulkan 1.3+ Backend: Utilizing modern features like Dynamic Rendering and Bindless Textures.

    GPU-Driven Instancing: A robust batching system that groups entities by MeshHandle, flattening scene data into contiguous SSBOs to draw thousands of objects in minimal draw calls.

    Entity Component System (EnTT): Leverages a high-performance ECS for game logic and scene management, ensuring a clean separation of concerns.

    Modern Descriptor Management: Decoupled descriptor set layouts (Global, Bindless, and Instance) to minimize state changes and remove unnecessary dependencies between shader stages.

    Render Graph Architecture: A modular graph-based approach to defining frame logic, allowing for easy expansion of post-processing and compute effects.

Design Patterns & Architecture

    State Separation: The engine utilizes a clear hierarchy of context objects—RenderContext (persistent), FrameContext (volatile), and SceneView (math/simulation)—unified under a single RenderState for pass execution.

    Persistent Cache Batching: The renderer avoids per-frame 'malloc' calls by utilizing persistent CPU-side caches for building render batches, ensuring 0% allocation overhead during steady-state rendering.

    Asset Handle System: Implements a strictly typed, linear handle system for Meshes and Textures, mapping file paths to unique IDs and GPU-side bindless slots.

    Bindless Rendering: Utilizing a global texture array to eliminate the need for per-material descriptor set rebinding, significantly reducing driver overhead.

Project Status

This engine is currently in active development. Current focus areas include GPU-based frustum culling, indirect drawing, and expanding the Render Graph to support advanced lighting models."
