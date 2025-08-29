#include "raylib.h"
#include <webxr.h>
#include <emscripten/emscripten.h>
#include <raymath.h>
#include <rlgl.h>

int screenWidth = 800;
int screenHeight = 600;
bool vrSessionActive = false;

EM_JS(void, console_log, (const char *msg), {
    console.log(UTF8ToString(msg));
});

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

void DrawScene(int eyeIndex = -1) {
    // Draw a ground plane
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
        [](void* userData, int time, float modelMatrix[16], WebXRView* views) {
            if (!vrSessionActive) {
                vrSessionActive = true;
                SetWindowSize(views[0].viewport[2] * 2, views[0].viewport[3]);
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
                
                // Clear only this eye's viewport area
                clear_viewport(viewport[0], viewport[1], viewport[2], viewport[3]);

                // Use the correct matrices for this eye
                Matrix eyeProjection = (eye == 0) ? leftProjectionMatrix : rightProjectionMatrix;
                Matrix eyeView = (eye == 0) ? leftViewMatrix : rightViewMatrix;
                
                rlSetMatrixProjection(eyeProjection);
                rlSetMatrixModelview(eyeView);

                // Draw the 3D scene with proper camera transforms for this eye
                DrawScene(eye);
                
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
            DrawScene();
            EndMode3D();
            
            DrawText("Press 'Launch VR' button to enter WebXR", 10, 10, 20, BLACK);
            DrawText("Desktop preview - VR will use proper stereo rendering", 10, 40, 16, DARKGRAY);
            
            EndDrawing();
        }
    }

    CloseWindow();
    return 0;
}