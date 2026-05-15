#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <functional>

/**
 * ============================================================================
 *  ARDIUM TITAN RUNTIME - MODULE 2: ARDIUM GRAPHICS
 * ============================================================================
 *  Architect: Major
 *  Version:   5.0.0
 *  
 *  Description:
 *  High-Performance Metal Rendering Engine.
 *  Features:
 *    - Triple-Buffered Render Loop (Max 3 frames in flight).
 *    - Zero-Copy Unified Memory Architecture.
 *    - Instanced Rendering for 1M+ particles.
 *    - Headless Fallback for CI/Server environments.
 * ============================================================================
 */

namespace Ardium {
namespace Graphics {

    // --- DATA STRUCTURES ---

    /**
     * @struct Vertex
     * @brief Represents a single vertex in 2D space.
     * Aligned to 16 bytes for GPU efficiency.
     */
    struct alignas(16) Vertex {
        float position[2]; // x, y
        float color[4];    // r, g, b, a
    };

    /**
     * @struct InstanceData
     * @brief Per-instance data for draw calls (Model Matrix, etc).
     */
    struct alignas(16) InstanceData {
        float transform[16]; // 4x4 Matrix
    };

    /**
     * @struct RenderState
     * @brief Encapsulates the state for a single frame.
     */
    struct RenderState {
        void* commandBuffer;       // id<MTLCommandBuffer>
        void* renderEncoder;       // id<MTLRenderCommandEncoder>
        uint32_t frameIndex;       // 0, 1, or 2 (Triple Buffering)
        double timestamp;          // Frame start time
    };

    // --- INTERFACE ---

    class IGraphicsEngine {
    public:
        virtual ~IGraphicsEngine() = default;

        /**
         * @brief Initialize the Metal Backend.
         * @return true if Metal initialized, false if Headless fallback active.
         */
        virtual bool Init() = 0;

        /**
         * @brief Shutdown the engine and release GPU resources.
         */
        virtual void Shutdown() = 0;

        /**
         * @brief Allocate a Zero-Copy Shared Buffer.
         * Memory is accessible by both CPU and GPU directly.
         * @param sizeBytes Size in bytes.
         * @return Raw pointer to CPU-mapped memory (or nullptr on fail).
         */
        virtual void* CreateSharedBuffer(size_t sizeBytes, uint64_t& outHandleID) = 0;

        /**
         * @brief Updates the content of a buffer (Synchronization point).
         * On Unified Memory, this might just be a barrier.
         */
        virtual void SyncBuffer(uint64_t handleID) = 0;

        // --- RENDER LOOP ---

        /**
         * @brief Begins a frame. Waits on semaphore if GPU is busy.
         * @return RenderState struct populated with encoders.
         */
        virtual RenderState BeginFrame() = 0;

        /**
         * @brief Ends the frame. Commits buffer and presents drawable.
         */
        virtual void EndFrame(const RenderState& state) = 0;

        /**
         * @brief Set the viewport/drawable size.
         */
        virtual void Resize(uint32_t width, uint32_t height) = 0;
    };

    /**
     * @brief Factory to create the engine instance.
     */
    std::unique_ptr<IGraphicsEngine> CreateGraphicsEngine();

} // namespace Graphics
} // namespace Ardium
