// qrcode.h
#pragma once

#include "node.h"
#include "server.h"
#include "camera.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "quirc.h"  // C library for QR code decoding
#ifdef __cplusplus
}
#endif

namespace ma::node {

/**
 * @class QRCodeNode
 * @brief A node that captures raw RGB888 video frames from a camera,
 *        decodes QR codes using the quirc library, and sends results via JSON.
 *
 * Features:
 * - Real-time QR code detection and decoding
 * - Supports multiple QR codes in one frame
 * - Optional base64-encoded JPEG output for debugging
 * - Configurable resolution and FPS
 * - Thread-safe with lifecycle management
 */
class QRCodeNode : public Node {
public:
    /**
     * @brief Constructor
     * @param id Unique identifier for this node instance
     */
    explicit QRCodeNode(std::string id);

    /**
     * @brief Destructor
     */
    ~QRCodeNode();

    /**
     * @brief Called when the node is created (before start)
     * @param config JSON configuration
     * @return MA_OK on success
     */
    ma_err_t onCreate(const json& config) override;

    /**
     * @brief Called when the node starts processing
     * @return MA_OK on success
     */
    ma_err_t onStart() override;

    /**
     * @brief Handle control commands (e.g., enable/disable, config update)
     * @param control Command name
     * @param data Command parameters
     * @return MA_OK on success
     */
    ma_err_t onControl(const std::string& control, const json& data) override;

    /**
     * @brief Stop processing
     * @return MA_OK on success
     */
    ma_err_t onStop() override;

    /**
     * @brief Destroy and release resources
     * @return MA_OK on success
     */
    ma_err_t onDestroy() override;

protected:
    /**
     * @brief Main processing loop (runs in a separate thread)
     */
    void threadEntry();

    /**
     * @brief Static stub to call threadEntry as C-style function
     * @param obj Pointer to this object
     */
    static void threadEntryStub(void* obj);

protected:
    int32_t count_;           ///< Running counter of processed frames
    Thread* thread_;          ///< Worker thread
    CameraNode* camera_;      ///< Connected camera node
    MessageBox raw_frame_;    ///< Message box to receive raw RGB888 frames
};

}  // namespace ma::node