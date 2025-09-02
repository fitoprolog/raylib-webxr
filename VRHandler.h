#pragma once

#include "raylib.h"
#include <webxr.h>
#include <functional>
#include <string>
#include <sstream>

class VRHandler {
public:
    using ControllerCallback = std::function<void(WebXRInputSource* source, int sourceId)>;
    using HandCallback = std::function<void(WebXRHandData* leftHand, WebXRHandData* rightHand)>;
    using SessionCallback = std::function<void()>;
    using ErrorCallback = std::function<void(int error)>;
    using FrameCallback = std::function<void(int time, float modelMatrix[16], WebXRView* views, void* handData)>;

private:
    bool vrSessionActive;
    bool handTrackingActive;
    bool isARSession;
    
    ControllerCallback controllerHandler;
    HandCallback handHandler;
    SessionCallback sessionStartHandler;
    SessionCallback sessionEndHandler;
    ErrorCallback errorHandler;
    FrameCallback frameHandler;

    static void onControllerSelect(WebXRInputSource* inputSource, void* userData);
    static void onControllerSelectStart(WebXRInputSource* inputSource, void* userData);
    static void onControllerSelectEnd(WebXRInputSource* inputSource, void* userData);

public:
    VRHandler();
    ~VRHandler();

    void initialize();
    void requestVRSession();
    void requestARSession();
    
    void setControllerHandler(ControllerCallback handler);
    void setHandHandler(HandCallback handler);
    void setSessionStartHandler(SessionCallback handler);
    void setSessionEndHandler(SessionCallback handler);
    void setErrorHandler(ErrorCallback handler);
    void setFrameHandler(FrameCallback handler);
    
    void processControllers();
    void processHands(void* handData);
    
    bool isVRSessionActive() const { return vrSessionActive; }
    bool isHandTrackingActive() const { return handTrackingActive; }
    bool isARSessionActive() const { return isARSession; }
    
    void drawControllers();
    void drawHands(void* handData);
    
    Matrix webXRToRaylibMatrix(float webxrMatrix[16]);
    Matrix invertWebXRViewMatrix(Matrix webxrViewMatrix);
    
    void drawHandJoint(Vector3 position, float radius, Color color);
    void drawHand(WebXRHandData* handData, Color color);
    
    void setViewport(int x, int y, int width, int height);
    void clearViewport(int x, int y, int width, int height);

    static VRHandler* getInstance() { return instance; }

    // Modern C++ logging methods
    static void log(const std::string& message);
    template<typename... Args>
    static void logf(const std::string& format, Args... args);

private:
    static VRHandler* instance;
    void setSessionActive(bool active) { vrSessionActive = active; }
    void setARSession(bool ar) { isARSession = ar; }
    void setHandTracking(bool active) { handTrackingActive = active; }
    
    friend void frameCallbackWrapper(void* userData, int time, float modelMatrix[16], WebXRView* views, void* handData);
    friend void sessionStartCallbackWrapper(void* userData);
    friend void sessionEndCallbackWrapper(void* userData);
    friend void errorCallbackWrapper(void* userData, int error);
};