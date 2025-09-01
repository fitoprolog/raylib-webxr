#ifndef WEBXR_H_
#define WEBXR_H_

/** @file
 * @brief Minimal WebXR Device API wrapper
 */

#ifdef __cplusplus
extern "C"
#endif
{

/** Errors enum */
enum WebXRError {
    WEBXR_ERR_API_UNSUPPORTED = -2, /**< WebXR Device API not supported in this browser */
    WEBXR_ERR_GL_INCAPABLE = -3, /**< GL context cannot render WebXR */
    WEBXR_ERR_SESSION_UNSUPPORTED = -4, /**< given session mode not supported */
};

/** WebXR handedness */
enum WebXRHandedness {
    WEBXR_HANDEDNESS_NONE = -1,
    WEBXR_HANDEDNESS_LEFT = 0,
    WEBXR_HANDEDNESS_RIGHT = 1,
};

/** WebXR target ray mode */
enum WebXRTargetRayMode {
    WEBXR_TARGET_RAY_MODE_GAZE = 0,
    WEBXR_TARGET_RAY_MODE_TRACKED_POINTER = 1,
    WEBXR_TARGET_RAY_MODE_SCREEN = 2,
};

/** WebXR 'XRSessionMode' enum*/
enum WebXRSessionMode {
    WEBXR_SESSION_MODE_INLINE = 0, /** "inline" */
    WEBXR_SESSION_MODE_IMMERSIVE_VR = 1, /** "immersive-vr" */
    WEBXR_SESSION_MODE_IMMERSIVE_AR = 2, /** "immersive-ar" */
};

/** WebXR view */
typedef struct WebXRView {
    /* view matrix */
    float viewMatrix[16];
    /* projection matrix */
    float projectionMatrix[16];
    /* x, y, width, height of the eye viewport on target texture */
    int viewport[4];
    float position[3];
    float rotation[4];
} WebXRView;

typedef struct WebXRInputSource {
    int id;
    WebXRHandedness handedness;
    WebXRTargetRayMode targetRayMode;
    int hasHand;        /**< 1 if this input source has hand tracking, 0 otherwise */
    int hasController;  /**< 1 if this input source has controller/gamepad, 0 otherwise */
} WebXRInputSource;

/** WebXR Hand Joint indices (matches the 25 joints in the WebXR Hand Input specification) */
enum WebXRHandJoint {
    WEBXR_HAND_JOINT_WRIST = 0,
    WEBXR_HAND_JOINT_THUMB_METACARPAL = 1,
    WEBXR_HAND_JOINT_THUMB_PHALANX_PROXIMAL = 2,
    WEBXR_HAND_JOINT_THUMB_PHALANX_DISTAL = 3,
    WEBXR_HAND_JOINT_THUMB_TIP = 4,
    WEBXR_HAND_JOINT_INDEX_FINGER_METACARPAL = 5,
    WEBXR_HAND_JOINT_INDEX_FINGER_PHALANX_PROXIMAL = 6,
    WEBXR_HAND_JOINT_INDEX_FINGER_PHALANX_INTERMEDIATE = 7,
    WEBXR_HAND_JOINT_INDEX_FINGER_PHALANX_DISTAL = 8,
    WEBXR_HAND_JOINT_INDEX_FINGER_TIP = 9,
    WEBXR_HAND_JOINT_MIDDLE_FINGER_METACARPAL = 10,
    WEBXR_HAND_JOINT_MIDDLE_FINGER_PHALANX_PROXIMAL = 11,
    WEBXR_HAND_JOINT_MIDDLE_FINGER_PHALANX_INTERMEDIATE = 12,
    WEBXR_HAND_JOINT_MIDDLE_FINGER_PHALANX_DISTAL = 13,
    WEBXR_HAND_JOINT_MIDDLE_FINGER_TIP = 14,
    WEBXR_HAND_JOINT_RING_FINGER_METACARPAL = 15,
    WEBXR_HAND_JOINT_RING_FINGER_PHALANX_PROXIMAL = 16,
    WEBXR_HAND_JOINT_RING_FINGER_PHALANX_INTERMEDIATE = 17,
    WEBXR_HAND_JOINT_RING_FINGER_PHALANX_DISTAL = 18,
    WEBXR_HAND_JOINT_RING_FINGER_TIP = 19,
    WEBXR_HAND_JOINT_PINKY_FINGER_METACARPAL = 20,
    WEBXR_HAND_JOINT_PINKY_FINGER_PHALANX_PROXIMAL = 21,
    WEBXR_HAND_JOINT_PINKY_FINGER_PHALANX_INTERMEDIATE = 22,
    WEBXR_HAND_JOINT_PINKY_FINGER_PHALANX_DISTAL = 23,
    WEBXR_HAND_JOINT_PINKY_FINGER_TIP = 24,
    WEBXR_HAND_JOINT_COUNT = 25
};

/** WebXR Hand Joint Pose (32 bytes: position[3] + rotation[4] + radius[1]) */
typedef struct WebXRHandJointPose {
    float position[3];    /**< Joint position in meters */
    float rotation[4];    /**< Joint rotation as quaternion (x, y, z, w) */
    float radius;         /**< Joint radius in meters */
} WebXRHandJointPose;

/** WebXR Hand data for one hand (25 joints) */
typedef struct WebXRHandData {
    WebXRHandJointPose joints[WEBXR_HAND_JOINT_COUNT];
} WebXRHandData;

/**
Callback for errors

@param userData User pointer passed to init_webxr()
@param error Error code
*/
typedef void (*webxr_error_callback_func)(void* userData, int error);

/**
Callback for frame rendering

@param userData User pointer passed to init_webxr()
@param time Current frame time
@param modelMatrix Transformation of the XR Device to tracking origin
@param views Array of two @ref WebXRView
@param handData Pointer to hand tracking data (2 hands * 25 joints * 32 bytes + 8 bytes for flags)
*/
typedef void (*webxr_frame_callback_func)(void* userData, int time, float modelMatrix[16], WebXRView views[2], void* handData);

/**
Callback for VR session start

@param userData User pointer passed to set_session_start_callback
*/
typedef void (*webxr_session_callback_func)(void* userData);

/**
Init WebXR rendering

@param mode Session mode from @ref WebXRSessionMode.
@param frameCallback Callback called every frame
@param sessionStartCallback Callback called when session is started
@param sessionEndCallback Callback called when session ended
@param errorCallback Callback called every frame
@param userData User data passed to the callbacks
*/
extern void webxr_init(
        WebXRSessionMode mode,
        webxr_frame_callback_func frameCallback,
        webxr_session_callback_func sessionStartCallback,
        webxr_session_callback_func sessionEndCallback,
        webxr_error_callback_func errorCallback,
        void* userData);

extern void webxr_set_session_blur_callback(
        webxr_session_callback_func sessionBlurCallback, void* userData);
extern void webxr_set_session_focus_callback(
        webxr_session_callback_func sessionFocusCallback, void* userData);

/*
Request session presentation start

Needs to be called from a [user activation event](https://html.spec.whatwg.org/multipage/interaction.html#triggered-by-user-activation).
*/
extern void webxr_request_session();

/*
Request that the webxr presentation exits VR mode
*/
extern void webxr_request_exit();

/**
Set projection matrix parameters for the webxr session

@param near Distance of near clipping plane
@param far Distance of far clipping plane
*/
extern void webxr_set_projection_params(float near, float far);

/**

WebXR Input

*/

/**
Callback for primary input action.

@param userData User pointer passed to @ref webxr_set_select_callback, @ref webxr_set_select_end_callback or @ref webxr_set_select_start_callback.
*/
typedef void (*webxr_input_callback_func)(WebXRInputSource* inputSource, void* userData);


/**
Set callbacks for primary input action.
*/
extern void webxr_set_select_callback(
        webxr_input_callback_func callback, void* userData);
extern void webxr_set_select_start_callback(
        webxr_input_callback_func callback, void* userData);
extern void webxr_set_select_end_callback(
        webxr_input_callback_func callback, void* userData);

/**
Get input sources.

@param outArray @ref WebXRInputSource array to fill.
@param max Size of outArray (in elements).
@param outCount Will receive the number of input sources valid in outArray.
*/
extern void webxr_get_input_sources(
        WebXRInputSource* outArray, int max, int* outCount);

/**
Get input pose. Can only be called during the frame callback.

@param source The source to get the pose for.
@param outPose Where to store the pose.
*/
extern void webxr_get_input_pose(WebXRInputSource* source, float* outMatrix);

/**
Check if hand tracking is currently supported and active.

@return 1 if hand tracking is supported, 0 otherwise
*/
extern int webxr_is_hand_tracking_supported();

/**
Get the pose of a specific hand joint. Can only be called during the frame callback.

@param handedness 0 for left hand, 1 for right hand
@param jointIndex Joint index from @ref WebXRHandJoint
@param outPosePtr Pointer to receive joint pose data (32 bytes: pos[3] + rot[4] + radius[1])
@return 1 if successful, 0 otherwise
*/
extern int webxr_get_hand_joint_pose(int handedness, int jointIndex, float* outPosePtr);

/**
Extract hand data from the frame callback hand data parameter.

@param handData Hand data pointer from frame callback
@param handedness 0 for left hand, 1 for right hand
@param outHandData Pointer to receive hand data
@return 1 if hand is detected, 0 otherwise
*/
static inline int webxr_get_hand_data(void* handData, int handedness, WebXRHandData* outHandData) {
    if (!handData || !outHandData) return 0;
    
    // Hand detection flags are at the end of the data
    int* flags = (int*)((char*)handData + (2 * 25 * 32));
    int leftHandDetected = flags[0];
    int rightHandDetected = flags[1];
    
    if ((handedness == 0 && !leftHandDetected) || (handedness == 1 && !rightHandDetected)) {
        return 0; // Hand not detected
    }
    
    // Copy hand data
    WebXRHandData* sourceHand = (WebXRHandData*)((char*)handData + (handedness * 25 * 32));
    *outHandData = *sourceHand;
    return 1;
}

/**
Check if the current session is an AR session.

@return 1 if AR session, 0 otherwise
*/
extern int webxr_is_ar_session();

/**
Request an AR session instead of VR. Must be called from user activation event.
*/
extern void webxr_request_ar_session();

}

#endif
