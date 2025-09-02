#include "raylib.h"
#include "VRHandler.h"
#include <webxr.h>
#include <emscripten/emscripten.h>
#include <raymath.h>
#include <rlgl.h>
#include <cstdio>

int screenWidth = 800;
int screenHeight = 600;
VRHandler* vrHandler = nullptr;

extern "C" EMSCRIPTEN_KEEPALIVE void launchit(){
    if (vrHandler) {
        vrHandler->requestVRSession();
    }
}

extern "C" EMSCRIPTEN_KEEPALIVE void launch_ar(){
    if (vrHandler) {
        vrHandler->requestARSession();
    }
}


void DrawScene(int eyeIndex = -1, void* handData = nullptr) {
    // Draw different background for AR vs VR
    if (!vrHandler || !vrHandler->isARSessionActive()) {
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
    
    // Draw VR controllers and hands if in VR session
    if (vrHandler && vrHandler->isVRSessionActive()) {
        vrHandler->drawControllers();
        vrHandler->drawHands(handData);
    }
}

int main(void)
{
    InitWindow(screenWidth, screenHeight, "WebXR Proper VR Rendering");
    
    // Initialize VR Handler
    vrHandler = new VRHandler();
    vrHandler->initialize();

    SetTargetFPS(90);

    // Set up frame callback for VR rendering
    vrHandler->setFrameHandler([](int time, float modelMatrix[16], WebXRView* views, void* handData) {
        if (!vrHandler->isVRSessionActive()) {
            SetWindowSize(views[0].viewport[2] * 2, views[0].viewport[3]);
        }

        // Extract projection matrices
        Matrix leftProjectionMatrix = vrHandler->webXRToRaylibMatrix(views[0].projectionMatrix);
        Matrix rightProjectionMatrix = vrHandler->webXRToRaylibMatrix(views[1].projectionMatrix);
        
        // Get the properly inverted view matrices for correct camera behavior
        Matrix leftViewMatrix = vrHandler->invertWebXRViewMatrix(vrHandler->webXRToRaylibMatrix(views[0].viewMatrix));
        Matrix rightViewMatrix = vrHandler->invertWebXRViewMatrix(vrHandler->webXRToRaylibMatrix(views[1].viewMatrix));

        // Render to each eye's viewport within the single WebXR framebuffer
        for (int eye = 0; eye < 2; eye++) {
            auto& viewport = views[eye].viewport;
            vrHandler->setViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            
            if (vrHandler->isARSessionActive()) {
                vrHandler->clearViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            } else {
                vrHandler->clearViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            }

            Matrix eyeProjection = (eye == 0) ? leftProjectionMatrix : rightProjectionMatrix;
            Matrix eyeView = (eye == 0) ? leftViewMatrix : rightViewMatrix;
            
            rlSetMatrixProjection(eyeProjection);
            rlSetMatrixModelview(eyeView);

            DrawScene(eye, handData);
            
            rlDrawRenderBatchActive();
        }
    });
    
    // Set up controller handler
    vrHandler->setControllerHandler([](WebXRInputSource* source, int sourceId) {
        // Custom controller processing can be added here
    });

    // Desktop fallback camera for testing
    Camera camera = {};
    camera.position = (Vector3){ 0.0f, 1.6f, 3.0f };
    camera.target = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    while (!WindowShouldClose()) {
        if (!vrHandler || !vrHandler->isVRSessionActive()) {
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

    delete vrHandler;
    CloseWindow();
    return 0;
}
