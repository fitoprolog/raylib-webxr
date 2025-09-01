#include "raylib.h"
#include <webxr.h>
#include <emscripten/emscripten.h>
#include <raymath.h>
#include <rlgl.h>
#include <cstdio>

int screenWidth = 800;
int screenHeight = 600;
bool vrSessionActive = false;
bool handTrackingActive = false;
bool isARSession = false;

EM_JS(void, console_log, (const char *msg), {
    console.log(UTF8ToString(msg));
});

// Controller input callbacks
void onControllerSelect(WebXRInputSource* inputSource, void* userData) {
    console_log("Controller SELECT pressed!");
    char msg[256];
    snprintf(msg, sizeof(msg), "SELECT - Controller ID: %d, Handedness: %s", 
             inputSource->id, 
             inputSource->handedness == WEBXR_HANDEDNESS_LEFT ? "Left" : 
             inputSource->handedness == WEBXR_HANDEDNESS_RIGHT ? "Right" : "None");
    console_log(msg);
}

void onControllerSelectStart(WebXRInputSource* inputSource, void* userData) {
    console_log("Controller SELECT START!");
    char msg[256];
    snprintf(msg, sizeof(msg), "SELECT START - Controller ID: %d, Handedness: %s, HasController: %d, HasHand: %d", 
             inputSource->id, 
             inputSource->handedness == WEBXR_HANDEDNESS_LEFT ? "Left" : 
             inputSource->handedness == WEBXR_HANDEDNESS_RIGHT ? "Right" : "None",
             inputSource->hasController,
             inputSource->hasHand);
    console_log(msg);
}

void onControllerSelectEnd(WebXRInputSource* inputSource, void* userData) {
    console_log("Controller SELECT END!");
    char msg[256];
    snprintf(msg, sizeof(msg), "SELECT END - Controller ID: %d, Handedness: %s", 
             inputSource->id, 
             inputSource->handedness == WEBXR_HANDEDNESS_LEFT ? "Left" : 
             inputSource->handedness == WEBXR_HANDEDNESS_RIGHT ? "Right" : "None");
    console_log(msg);
}

EM_JS(void, init_webgl_context, (), {
    if (typeof gl === 'undefined' || !gl) {
        window.gl = Module.ctx || document.querySelector('canvas').getContext("webgl", { xrCompatible: true });
        console.log("WebGL context initialized:", window.gl ? "success" : "failed");
    }
});

extern "C" EMSCRIPTEN_KEEPALIVE void launchit(){
    init_webgl_context();
    webxr_request_session();
}

extern "C" EMSCRIPTEN_KEEPALIVE void launch_ar(){
    init_webgl_context();
    webxr_request_ar_session();
}

EM_JS(void, set_viewport, (int x, int y, int width, int height), {
    if (typeof gl === 'undefined') {
        gl = window.gl || Module.ctx;
    }
    if (gl) {
        gl.viewport(x, y, width, height);
    } else {
        console.error("WebGL context not available for viewport setting");
    }
});

EM_JS(void, clear_viewport, (int x, int y, int width, int height), {
    if (typeof gl === 'undefined') {
        gl = window.gl || Module.ctx;
    }
    if (gl) {
        // Enable scissor test to clear only this viewport
        gl.enable(gl.SCISSOR_TEST);
        gl.scissor(x, y, width, height);
        gl.clearColor(0.4, 0.7, 1.0, 1.0); // Sky blue
        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
        gl.disable(gl.SCISSOR_TEST);
    }
});

// Convert WebXR 16-element array to raylib Matrix
Matrix WebXRToRaylibMatrix(float webxrMatrix[16]) {
    Matrix result;
    result.m0 = webxrMatrix[0];   result.m4 = webxrMatrix[4];   result.m8 = webxrMatrix[8];    result.m12 = webxrMatrix[12];
    result.m1 = webxrMatrix[1];   result.m5 = webxrMatrix[5];   result.m9 = webxrMatrix[9];    result.m13 = webxrMatrix[13];
    result.m2 = webxrMatrix[2];   result.m6 = webxrMatrix[6];   result.m10 = webxrMatrix[10];  result.m14 = webxrMatrix[14];
    result.m3 = webxrMatrix[3];   result.m7 = webxrMatrix[7];   result.m11 = webxrMatrix[11];  result.m15 = webxrMatrix[15];
    return result;
}

// WebXR provides view matrices that need to be inverted for proper camera behavior
Matrix InvertWebXRViewMatrix(Matrix webxrViewMatrix) {
    // WebXR view matrices are camera-to-world transforms
    // We need world-to-camera (inverse) for proper rendering
    return MatrixInvert(webxrViewMatrix);
}

void DrawHandJoint(Vector3 position, float radius, Color color) {
    DrawSphere(position, radius, color);
}

void DrawHand(WebXRHandData* handData, Color color) {
    if (!handData) return;
    
    // Draw all joints
    for (int i = 0; i < WEBXR_HAND_JOINT_COUNT; i++) {
        Vector3 pos = {
            handData->joints[i].position[0],
            handData->joints[i].position[1],
            handData->joints[i].position[2]
        };
        float radius = handData->joints[i].radius;
        if (radius > 0.0f) {  // Only draw if radius is valid
            DrawHandJoint(pos, radius, color);
        }
    }
    
    // Draw finger bones as lines
    static const int fingerStarts[5] = {1, 5, 10, 15, 20};  // Start indices for each finger
    static const int fingerLengths[5] = {4, 5, 5, 5, 5};   // Number of joints per finger
    
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
        
        // Connect finger to wrist
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

void DrawControllers() {
    // Get input sources and draw controller spheres
    WebXRInputSource inputSources[16];
    int inputCount = 0;
    
    webxr_get_input_sources(inputSources, 16, &inputCount);
    
    for (int i = 0; i < inputCount; i++) {
        WebXRInputSource* source = &inputSources[i];
        
        // Only draw controllers (not hands)
        if (source->hasController) {
            float poseMatrix[16];
            webxr_get_input_pose(source, poseMatrix);
            
            // Extract position from the pose matrix (translation is in indices 12, 13, 14)
            Vector3 controllerPos = {
                poseMatrix[12],
                poseMatrix[13], 
                poseMatrix[14]
            };
            
            // Choose color based on handedness
            Color controllerColor = GRAY;
            if (source->handedness == WEBXR_HANDEDNESS_LEFT) {
                controllerColor = PURPLE;  // Left controller = purple
            } else if (source->handedness == WEBXR_HANDEDNESS_RIGHT) {
                controllerColor = ORANGE;  // Right controller = orange
            }
            
            // Draw controller as a sphere
            DrawSphere(controllerPos, 0.05f, controllerColor);
            DrawSphereWires(controllerPos, 0.05f, 8, 16, BLACK);
            
            // Draw a line showing controller forward direction
            Vector3 forward = {
                controllerPos.x + poseMatrix[8] * 0.2f,  // Forward vector from rotation matrix
                controllerPos.y + poseMatrix[9] * 0.2f,
                controllerPos.z + poseMatrix[10] * 0.2f
            };
            DrawLine3D(controllerPos, forward, controllerColor);
        }
    }
}

void DrawScene(int eyeIndex = -1, void* handData = nullptr) {
    // Draw different background for AR vs VR
    if (!isARSession) {
        // Draw a ground plane for VR
        DrawPlane((Vector3){ 0.0f, 0.0f, 0.0f }, (Vector2){ 20.0f, 20.0f }, GREEN);
        
        // Draw some cubes at different positions
        DrawCube((Vector3){ 0.0f, 0.5f, -3.0f }, 1.0f, 1.0f, 1.0f, RED);
        DrawCube((Vector3){ 2.0f, 0.5f, -5.0f }, 1.0f, 1.0f, 1.0f, BLUE);
        DrawCube((Vector3){ -2.0f, 0.5f, -4.0f }, 1.0f, 1.0f, 1.0f, YELLOW);
        
        // Draw cube wireframes for better visibility
        DrawCubeWires((Vector3){ 0.0f, 0.5f, -3.0f }, 1.0f, 1.0f, 1.0f, MAROON);
        DrawCubeWires((Vector3){ 2.0f, 0.5f, -5.0f }, 1.0f, 1.0f, 1.0f, DARKBLUE);
        DrawCubeWires((Vector3){ -2.0f, 0.5f, -4.0f }, 1.0f, 1.0f, 1.0f, ORANGE);
        
        // Add a reference grid
        DrawGrid(20, 1.0f);
    } else {
        // For AR, draw minimal virtual content that augments reality
        DrawCube((Vector3){ 0.0f, 0.0f, -1.0f }, 0.2f, 0.2f, 0.2f, (Color){255, 0, 0, 128});
    }
    
    // Draw VR controllers if in VR session
    if (vrSessionActive) {
        DrawControllers();
    }
    
    // Draw hand tracking if available
    if (handTrackingActive && handData) {
        WebXRHandData leftHand, rightHand;
        
        // Draw left hand
        if (webxr_get_hand_data(handData, 0, &leftHand)) {
            DrawHand(&leftHand, BLUE);
        }
        
        // Draw right hand  
        if (webxr_get_hand_data(handData, 1, &rightHand)) {
            DrawHand(&rightHand, RED);
        }
    }
}

int main(void)
{
    InitWindow(screenWidth, screenHeight, "WebXR Proper VR Rendering");
    
    // Initialize WebGL context for WebXR
    init_webgl_context();

    SetTargetFPS(90);

    webxr_init(
        WEBXR_SESSION_MODE_IMMERSIVE_VR,
        /* Frame callback */
        [](void* userData, int time, float modelMatrix[16], WebXRView* views, void* handData) {
            if (!vrSessionActive) {
                vrSessionActive = true;
                SetWindowSize(views[0].viewport[2] * 2, views[0].viewport[3]);
                
                // Check if this is an AR session
                isARSession = webxr_is_ar_session();
                
                // Check for hand tracking support
                handTrackingActive = webxr_is_hand_tracking_supported();
                
                if (handTrackingActive) {
                    console_log("Hand tracking is active!");
                } else {
                    console_log("Hand tracking not available, will show controllers");
                }
                
                if (isARSession) {
                    console_log("AR session started");
                } else {
                    console_log("VR session started");
                }
                
                // Debug: Check input sources
                WebXRInputSource inputSources[16];
                int inputCount = 0;
                webxr_get_input_sources(inputSources, 16, &inputCount);
                
                char debugMsg[256];
                snprintf(debugMsg, sizeof(debugMsg), "Detected %d input sources", inputCount);
                console_log(debugMsg);
                
                for (int i = 0; i < inputCount; i++) {
                    snprintf(debugMsg, sizeof(debugMsg), "Input %d: Handedness=%d, HasController=%d, HasHand=%d", 
                             i, inputSources[i].handedness, inputSources[i].hasController, inputSources[i].hasHand);
                    console_log(debugMsg);
                }
            }
            

            // Don't use raylib's BeginDrawing/EndDrawing - manage WebGL directly
            
            // Extract projection matrices (these are correct)
            Matrix leftProjectionMatrix = WebXRToRaylibMatrix(views[0].projectionMatrix);
            Matrix rightProjectionMatrix = WebXRToRaylibMatrix(views[1].projectionMatrix);
            
            // Get the properly inverted view matrices for correct camera behavior
            Matrix leftViewMatrix = InvertWebXRViewMatrix(WebXRToRaylibMatrix(views[0].viewMatrix));
            Matrix rightViewMatrix = InvertWebXRViewMatrix(WebXRToRaylibMatrix(views[1].viewMatrix));

            // Render to each eye's viewport within the single WebXR framebuffer
            for (int eye = 0; eye < 2; eye++) {
                // Set viewport for this eye
                auto& viewport = views[eye].viewport;
                set_viewport(viewport[0], viewport[1], viewport[2], viewport[3]);
                
                // Clear only this eye's viewport area (different for AR)
                if (isARSession) {
                    // For AR, clear with transparent background to show camera feed
                    clear_viewport(viewport[0], viewport[1], viewport[2], viewport[3]);
                } else {
                    // For VR, clear with sky blue background
                    clear_viewport(viewport[0], viewport[1], viewport[2], viewport[3]);
                }

                // Use the correct matrices for this eye
                Matrix eyeProjection = (eye == 0) ? leftProjectionMatrix : rightProjectionMatrix;
                Matrix eyeView = (eye == 0) ? leftViewMatrix : rightViewMatrix;
                
                rlSetMatrixProjection(eyeProjection);
                rlSetMatrixModelview(eyeView);

                // Draw the 3D scene with proper camera transforms for this eye
                DrawScene(eye, handData);
                
                // Make sure everything gets drawn
                rlDrawRenderBatchActive();
            }
        },
        [](void* userData) {
            console_log("WebXR session started");
            vrSessionActive = true;
        },
        [](void* userData) {
            console_log("WebXR session ended");
            vrSessionActive = false;
        },
        [](void* userData, int error) {
            console_log("WebXR error occurred");
        },
        nullptr
    );
    
    // Set up controller input callbacks
    webxr_set_select_callback(onControllerSelect, nullptr);
    webxr_set_select_start_callback(onControllerSelectStart, nullptr);
    webxr_set_select_end_callback(onControllerSelectEnd, nullptr);

    // Desktop fallback camera for testing
    Camera camera = {};
    camera.position = (Vector3){ 0.0f, 1.6f, 3.0f };
    camera.target = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    while (!WindowShouldClose()) {
        if (!vrSessionActive) {
            // Desktop fallback rendering
            BeginDrawing();
            ClearBackground(SKYBLUE);
            
            BeginMode3D(camera);
            DrawScene(-1, nullptr);
            EndMode3D();
            
            DrawText("Press 'Launch VR' or 'Launch AR' button to enter WebXR", 10, 10, 20, BLACK);
            DrawText("Desktop preview - VR will use proper stereo rendering", 10, 40, 16, DARKGRAY);
            DrawText("Features: Hand Tracking + Controller Support + AR", 10, 65, 16, DARKGRAY);
            DrawText("Controllers: Purple=Left, Orange=Right spheres", 10, 90, 14, BLUE);
            DrawText("Hands: Blue=Left, Red=Right joint tracking", 10, 110, 14, BLUE);
            
            EndDrawing();
        }
    }

    CloseWindow();
    return 0;
}
