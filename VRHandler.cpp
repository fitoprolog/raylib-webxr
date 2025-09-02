#include "VRHandler.h"
#include <emscripten/emscripten.h>
#include <raymath.h>
#include <rlgl.h>
#include <iomanip>
#include <sstream>

VRHandler* VRHandler::instance = nullptr;

EM_JS(void, console_log_vr, (const char *msg), {
    console.log(UTF8ToString(msg));
});

EM_JS(void, init_webgl_context_vr, (), {
    if (typeof gl === 'undefined' || !gl) {
        window.gl = Module.ctx || document.querySelector('canvas').getContext("webgl", { xrCompatible: true });
        console.log("WebGL context initialized:", window.gl ? "success" : "failed");
    }
});

EM_JS(void, set_viewport_vr, (int x, int y, int width, int height), {
    if (typeof gl === 'undefined') {
        gl = window.gl || Module.ctx;
    }
    if (gl) {
        gl.viewport(x, y, width, height);
    } else {
        console.error("WebGL context not available for viewport setting");
    }
});

EM_JS(void, clear_viewport_vr, (int x, int y, int width, int height), {
    if (typeof gl === 'undefined') {
        gl = window.gl || Module.ctx;
    }
    if (gl) {
        gl.enable(gl.SCISSOR_TEST);
        gl.scissor(x, y, width, height);
        gl.clearColor(0.4, 0.7, 1.0, 1.0);
        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
        gl.disable(gl.SCISSOR_TEST);
    }
});

void frameCallbackWrapper(void* userData, int time, float modelMatrix[16], WebXRView* views, void* handData) {
    VRHandler* handler = VRHandler::getInstance();
    if (handler && handler->frameHandler) {
        if (!handler->vrSessionActive) {
            handler->setSessionActive(true);
            handler->setARSession(webxr_is_ar_session());
            handler->setHandTracking(webxr_is_hand_tracking_supported());
            
            if (handler->handTrackingActive) {
                VRHandler::log("Hand tracking is active!");
            } else {
                VRHandler::log("Hand tracking not available, will show controllers");
            }
            
            if (handler->isARSession) {
                VRHandler::log("AR session started");
            } else {
                VRHandler::log("VR session started");
            }
            
            WebXRInputSource inputSources[16];
            int inputCount = 0;
            webxr_get_input_sources(inputSources, 16, &inputCount);
            
            std::ostringstream oss;
            oss << "Detected " << inputCount << " input sources";
            VRHandler::log(oss.str());
            
            for (int i = 0; i < inputCount; i++) {
                std::ostringstream inputOss;
                inputOss << "Input " << i << ": Handedness=" << inputSources[i].handedness 
                        << ", HasController=" << inputSources[i].hasController 
                        << ", HasHand=" << inputSources[i].hasHand;
                VRHandler::log(inputOss.str());
            }
        }
        handler->frameHandler(time, modelMatrix, views, handData);
    }
}

void sessionStartCallbackWrapper(void* userData) {
    VRHandler* handler = VRHandler::getInstance();
    if (handler) {
        VRHandler::log("WebXR session started");
        handler->setSessionActive(true);
        if (handler->sessionStartHandler) {
            handler->sessionStartHandler();
        }
    }
}

void sessionEndCallbackWrapper(void* userData) {
    VRHandler* handler = VRHandler::getInstance();
    if (handler) {
        VRHandler::log("WebXR session ended");
        handler->setSessionActive(false);
        if (handler->sessionEndHandler) {
            handler->sessionEndHandler();
        }
    }
}

void errorCallbackWrapper(void* userData, int error) {
    VRHandler* handler = VRHandler::getInstance();
    if (handler) {
        VRHandler::log("WebXR error occurred");
        if (handler->errorHandler) {
            handler->errorHandler(error);
        }
    }
}

VRHandler::VRHandler() : vrSessionActive(false), handTrackingActive(false), isARSession(false) {
    instance = this;
}

VRHandler::~VRHandler() {
    if (instance == this) {
        instance = nullptr;
    }
}

void VRHandler::initialize() {
    init_webgl_context_vr();
    
    webxr_init(
        WEBXR_SESSION_MODE_IMMERSIVE_VR,
        frameCallbackWrapper,
        sessionStartCallbackWrapper,
        sessionEndCallbackWrapper,
        errorCallbackWrapper,
        nullptr
    );
    
    webxr_set_select_callback(onControllerSelect, nullptr);
    webxr_set_select_start_callback(onControllerSelectStart, nullptr);
    webxr_set_select_end_callback(onControllerSelectEnd, nullptr);
}

void VRHandler::requestVRSession() {
    init_webgl_context_vr();
    webxr_request_session();
}

void VRHandler::requestARSession() {
    init_webgl_context_vr();
    webxr_request_ar_session();
}

void VRHandler::setControllerHandler(ControllerCallback handler) {
    controllerHandler = handler;
}

void VRHandler::setHandHandler(HandCallback handler) {
    handHandler = handler;
}

void VRHandler::setSessionStartHandler(SessionCallback handler) {
    sessionStartHandler = handler;
}

void VRHandler::setSessionEndHandler(SessionCallback handler) {
    sessionEndHandler = handler;
}

void VRHandler::setErrorHandler(ErrorCallback handler) {
    errorHandler = handler;
}

void VRHandler::setFrameHandler(FrameCallback handler) {
    frameHandler = handler;
}

void VRHandler::processControllers() {
    WebXRInputSource inputSources[16];
    int inputCount = 0;
    
    webxr_get_input_sources(inputSources, 16, &inputCount);
    
    for (int i = 0; i < inputCount; i++) {
        WebXRInputSource* source = &inputSources[i];
        if (controllerHandler && source->hasController) {
            controllerHandler(source, i);
        }
    }
}

void VRHandler::processHands(void* handData) {
    if (handHandler && handTrackingActive && handData) {
        WebXRHandData leftHand, rightHand;
        WebXRHandData* leftHandPtr = nullptr;
        WebXRHandData* rightHandPtr = nullptr;
        
        if (webxr_get_hand_data(handData, 0, &leftHand)) {
            leftHandPtr = &leftHand;
        }
        
        if (webxr_get_hand_data(handData, 1, &rightHand)) {
            rightHandPtr = &rightHand;
        }
        
        handHandler(leftHandPtr, rightHandPtr);
    }
}

void VRHandler::drawControllers() {
    WebXRInputSource inputSources[16];
    int inputCount = 0;
    
    webxr_get_input_sources(inputSources, 16, &inputCount);
    
    for (int i = 0; i < inputCount; i++) {
        WebXRInputSource* source = &inputSources[i];
        
        if (source->hasController) {
            float poseMatrix[16];
            webxr_get_input_pose(source, poseMatrix);
            
            Vector3 controllerPos = {
                poseMatrix[12],
                poseMatrix[13], 
                poseMatrix[14]
            };
            
            Color controllerColor = GRAY;
            if (source->handedness == WEBXR_HANDEDNESS_LEFT) {
                controllerColor = PURPLE;
            } else if (source->handedness == WEBXR_HANDEDNESS_RIGHT) {
                controllerColor = ORANGE;
            }
            
            DrawSphere(controllerPos, 0.05f, controllerColor);
            DrawSphereWires(controllerPos, 0.05f, 8, 16, BLACK);
            
            Vector3 forward = {
                controllerPos.x + poseMatrix[8] * 0.2f,
                controllerPos.y + poseMatrix[9] * 0.2f,
                controllerPos.z + poseMatrix[10] * 0.2f
            };
            DrawLine3D(controllerPos, forward, controllerColor);
        }
    }
}

void VRHandler::drawHands(void* handData) {
    if (handTrackingActive && handData) {
        WebXRHandData leftHand, rightHand;
        
        if (webxr_get_hand_data(handData, 0, &leftHand)) {
            drawHand(&leftHand, BLUE);
        }
        
        if (webxr_get_hand_data(handData, 1, &rightHand)) {
            drawHand(&rightHand, RED);
        }
    }
}

Matrix VRHandler::webXRToRaylibMatrix(float webxrMatrix[16]) {
    Matrix result;
    result.m0 = webxrMatrix[0];   result.m4 = webxrMatrix[4];   result.m8 = webxrMatrix[8];    result.m12 = webxrMatrix[12];
    result.m1 = webxrMatrix[1];   result.m5 = webxrMatrix[5];   result.m9 = webxrMatrix[9];    result.m13 = webxrMatrix[13];
    result.m2 = webxrMatrix[2];   result.m6 = webxrMatrix[6];   result.m10 = webxrMatrix[10];  result.m14 = webxrMatrix[14];
    result.m3 = webxrMatrix[3];   result.m7 = webxrMatrix[7];   result.m11 = webxrMatrix[11];  result.m15 = webxrMatrix[15];
    return result;
}

Matrix VRHandler::invertWebXRViewMatrix(Matrix webxrViewMatrix) {
    return MatrixInvert(webxrViewMatrix);
}

void VRHandler::drawHandJoint(Vector3 position, float radius, Color color) {
    DrawSphere(position, radius, color);
}

void VRHandler::drawHand(WebXRHandData* handData, Color color) {
    if (!handData) return;
    
    for (int i = 0; i < WEBXR_HAND_JOINT_COUNT; i++) {
        Vector3 pos = {
            handData->joints[i].position[0],
            handData->joints[i].position[1],
            handData->joints[i].position[2]
        };
        float radius = handData->joints[i].radius;
        if (radius > 0.0f) {
            drawHandJoint(pos, radius, color);
        }
    }
    
    static const int fingerStarts[5] = {1, 5, 10, 15, 20};
    static const int fingerLengths[5] = {4, 5, 5, 5, 5};
    
    for (int finger = 0; finger < 5; finger++) {
        int start = fingerStarts[finger];
        int length = fingerLengths[finger];
        
        for (int j = 0; j < length - 1; j++) {
            int joint1 = start + j;
            int joint2 = start + j + 1;
            
            Vector3 pos1 = {
                handData->joints[joint1].position[0],
                handData->joints[joint1].position[1],
                handData->joints[joint1].position[2]
            };
            Vector3 pos2 = {
                handData->joints[joint2].position[0],
                handData->joints[joint2].position[1],
                handData->joints[joint2].position[2]
            };
            
            if (handData->joints[joint1].radius > 0.0f && handData->joints[joint2].radius > 0.0f) {
                DrawLine3D(pos1, pos2, color);
            }
        }
        
        if (finger > 0 && handData->joints[0].radius > 0.0f && handData->joints[start].radius > 0.0f) {
            Vector3 wristPos = {
                handData->joints[0].position[0],
                handData->joints[0].position[1],
                handData->joints[0].position[2]
            };
            Vector3 fingerStart = {
                handData->joints[start].position[0],
                handData->joints[start].position[1],
                handData->joints[start].position[2]
            };
            DrawLine3D(wristPos, fingerStart, color);
        }
    }
}

void VRHandler::onControllerSelect(WebXRInputSource* inputSource, void* userData) {
    VRHandler* handler = VRHandler::getInstance();
    if (handler && handler->controllerHandler) {
        log("Controller SELECT pressed!");
        
        std::string handedness = (inputSource->handedness == WEBXR_HANDEDNESS_LEFT) ? "Left" : 
                                (inputSource->handedness == WEBXR_HANDEDNESS_RIGHT) ? "Right" : "None";
        
        std::ostringstream oss;
        oss << "SELECT - Controller ID: " << inputSource->id << ", Handedness: " << handedness;
        log(oss.str());
    }
}

void VRHandler::onControllerSelectStart(WebXRInputSource* inputSource, void* userData) {
    VRHandler* handler = VRHandler::getInstance();
    if (handler && handler->controllerHandler) {
        log("Controller SELECT START!");
        
        std::string handedness = (inputSource->handedness == WEBXR_HANDEDNESS_LEFT) ? "Left" : 
                                (inputSource->handedness == WEBXR_HANDEDNESS_RIGHT) ? "Right" : "None";
        
        std::ostringstream oss;
        oss << "SELECT START - Controller ID: " << inputSource->id 
            << ", Handedness: " << handedness
            << ", HasController: " << inputSource->hasController 
            << ", HasHand: " << inputSource->hasHand;
        log(oss.str());
    }
}

void VRHandler::onControllerSelectEnd(WebXRInputSource* inputSource, void* userData) {
    VRHandler* handler = VRHandler::getInstance();
    if (handler && handler->controllerHandler) {
        log("Controller SELECT END!");
        
        std::string handedness = (inputSource->handedness == WEBXR_HANDEDNESS_LEFT) ? "Left" : 
                                (inputSource->handedness == WEBXR_HANDEDNESS_RIGHT) ? "Right" : "None";
        
        std::ostringstream oss;
        oss << "SELECT END - Controller ID: " << inputSource->id << ", Handedness: " << handedness;
        log(oss.str());
    }
}

void VRHandler::setViewport(int x, int y, int width, int height) {
    set_viewport_vr(x, y, width, height);
}

void VRHandler::clearViewport(int x, int y, int width, int height) {
    clear_viewport_vr(x, y, width, height);
}

void VRHandler::log(const std::string& message) {
    console_log_vr(message.c_str());
}