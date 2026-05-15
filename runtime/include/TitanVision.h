#pragma once

#include "ArdiumOS.h"
#include "TitanObject.h"
#include <string>
#include <vector>
#include <memory>

/**
 * ============================================================================
 *  ARDIUM TITAN RUNTIME - MODULE 4: TITAN VISION & AI
 * ============================================================================
 *  Architect: Major
 *  Version:   5.0.0
 *  
 *  Description:
 *  Integrates Apple's Vision and CoreML frameworks into the Titan architecture.
 *  Provides real-time feature detection and neural network inference.
 * ============================================================================
 */

namespace Ardium {
namespace Titan {

    /**
     * @struct DetectionResult
     * @brief Normalized representation of AI detection metadata.
     */
    struct DetectionResult {
        std::string label;
        float confidence;
        float x, y, width, height; // Normalized 0.0 - 1.0
    };

    /**
     * @class VisionEngine
     * @brief Orchestrates hardware-accelerated computer vision tasks.
     */
    class VisionEngine {
    public:
        virtual ~VisionEngine() = default;

        /**
         * @brief Initialize Vision/CoreML subsystem.
         */
        virtual void Init() = 0;

        /**
         * @brief Load a CoreML model from path.
         */
        virtual bool LoadModel(const std::string& modelPath) = 0;

        /**
         * @brief Perform face detection on a raw image buffer.
         * Results are published to the Titan Unified Bus.
         */
        virtual void DetectFaces(void* pixelBuffer, uint32_t w, uint32_t h) = 0;

        /**
         * @brief Execute generic inference using the loaded model.
         */
        virtual std::vector<DetectionResult> Predict(void* inputData, size_t size) = 0;
    };

    /**
     * @brief Factory to create the Vision Engine instance.
     */
    std::unique_ptr<VisionEngine> CreateVisionEngine();

} // namespace Titan
} // namespace Ardium
