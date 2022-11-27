#include "raylib.h"
#include <stdlib.h>
#include <webxr.h>
#include <stdio.h>
#include <emscripten/emscripten.h>
#include <string.h>
#include <raymath.h>
#include <stdlib.h>
#define GLSL_VERSION            100
#include<string>
Camera cameras[2] = { 0 };
RenderTexture  textures[2];
int screenWidth = 800;
int screenHeight = 600;
bool texturesCreated = false;

EM_JS(void, console_log_int, (), {
    console.log("THIS BITCH STARTED");
    gl = document.querySelector('canvas').getContext("webgl", { xrCompatible: true });
});

EM_JS(void, fucking_error, (int error),{
    console.log("Fucking error was",error);
})

EM_JS(void, logger, (const char *msg),{
    console.log(UTF8ToString(msg));
})

extern "C" EMSCRIPTEN_KEEPALIVE void launchit(){
  webxr_request_session();
}

EM_JS(void,set_viewport,(int x,int y, int width, int height),{
   gl.viewport(x, y,width,height);  
})


int main(void)
{

  InitWindow(screenWidth, screenHeight, "webxr");

  // Define the camera to look into our 3d world
  for (Camera& camera : cameras){
    camera.position = (Vector3){ 10.0f, 10.0f, 10.0f }; 
    camera.target = (Vector3){ 0.0f, 1.0f, 100.0f };      
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };         
    camera.fovy = 89/2;                              
    camera.projection = CAMERA_PERSPECTIVE;     
  }

  Model model = LoadModel("resources/models/iqm/guy.iqm");                    // Load the animated model mesh and basic data
  Texture2D texture = LoadTexture("resources/models/iqm/guytex.png");         // Load model texture and set material
  SetMaterialTexture(&model.materials[0], MATERIAL_MAP_DIFFUSE, texture);     // Set model material map texture

  Vector3 position = { 0.0f, 0.0f, 0.0f };            // Set model position

  // Load animation data
  unsigned int animsCount = 0;
  ModelAnimation *anims = LoadModelAnimations("resources/models/iqm/guyanim.iqm", &animsCount);
  int animFrameCounter = 0;

  SetCameraMode(cameras[0], CAMERA_FIRST_PERSON); 
  SetCameraMode(cameras[1], CAMERA_FIRST_PERSON); 

  SetTargetFPS(120);                   
                                      
 webxr_init(
      WEBXR_SESSION_MODE_IMMERSIVE_VR,
      /* Frame callback */
      [](void* userData, int, float[16], WebXRView* views) {
      if (!texturesCreated){
        texturesCreated=true;
        SetWindowSize(views[0].viewport[2]*2,views[0].viewport[3]);
      }
      Vector3 basePos = { 0.0f, 0.0f, 0.0f };
      BeginDrawing();
      ClearBackground(PINK);
      Matrix mat;
      for (char i=0; i!= 2; i++){
        auto& viewport = views[i].viewport;
        set_viewport(viewport[0],
                 viewport[1],
                 viewport[2],
                 viewport[3]);
        auto& position = views[i].position;
        auto& rotation = views[i].rotation;

        cameras[i].position= (Vector3){-position[0],
                                       -position[1],
                                       -position[2]};
        cameras[i].fovy = (1/(2.0*atan(views[i].projectionMatrix[5] )))/PI*180;
        logger((std::string("FOV:")+std::to_string(cameras[i].fovy)).c_str());
        Quaternion headsetRotation = Quaternion{rotation[0],rotation[1],rotation[2],rotation[3]};
        cameras[i].target=Vector3RotateByQuaternion(Vector3{0,0,-1},headsetRotation);

        cameras[i].up = Vector3RotateByQuaternion((Vector3){ 0.0f, 1.0f, 0.0f },headsetRotation);
        BeginMode3D(cameras[i]);
        DrawCube((Vector3){-0.0f, 0.0f, -18.0f}, 3.0f, 5.0f, 3.0f, RED);
        EndMode3D();
      }
      EndDrawing();

      },
      [](void* userData) {
        screenWidth = GetScreenWidth();
        screenHeight = GetScreenHeight();
        fucking_error(screenWidth);
        console_log_int();
      },
      [](void* userData) {
      },
      [](void* userData, int error) {
        fucking_error(error);
      },
      0);


  while (!WindowShouldClose());        // Detect window close button or ESC key

  // De-Initialization
  //--------------------------------------------------------------------------------------
  UnloadTexture(texture);     // Unload texture

  // Unload model animations data
  for (unsigned int i = 0; i < animsCount; i++) UnloadModelAnimation(anims[i]);
  RL_FREE(anims);

  UnloadModel(model);         // Unload model

  CloseWindow();              // Close window and OpenGL context
                              //--------------------------------------------------------------------------------------

}

