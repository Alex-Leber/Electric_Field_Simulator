#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Required for the web build
#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
    #include <emscripten/html5.h>
#endif

#define MAX_CHARGES 100
#define FIELD_LINE_STEP_SIZE 0.05f

typedef struct Charge {
    Vector3 position;
    float value;
} Charge;

// GLOBAL STATE VARIABLES
const int initialWidth = 1920;
const int initialHeight = 1080;

Camera3D camera = { 0 };
Charge charges[MAX_CHARGES];
int numCharges = 0;
int selectedCharge = -1;
bool freeCameraMode = true;

// Custom Camera State
float cameraYaw = 0.0f;
float cameraPitch = 0.0f;
bool isCameraFirstFrame = true;

// Simulation Settings
int fieldLineSteps = 3000;
int lineResolution = 3;

// UI State
char chargeInput[16] = "";
int inputLength = 0;
bool isTyping = false;

// Helper Functions

void DrawInfiniteGrid() {
    int slices = 100;
    float spacing = 1.0f;
    float halfSize = (slices * spacing) / 2.0f;

    rlBegin(RL_LINES);
    rlColor4ub(40, 40, 40, 255);

    for (int i = 0; i <= slices; i++) {
        float pos = -halfSize + (i * spacing);
        rlVertex3f(pos, 0.0f, -halfSize);
        rlVertex3f(pos, 0.0f, halfSize);
        rlVertex3f(-halfSize, 0.0f, pos);
        rlVertex3f(halfSize, 0.0f, pos);
    }
    rlEnd();
}

bool GetGroundIntersection(Ray ray, Vector3 *outPos) {
    if (fabsf(ray.direction.y) < 0.001f) return false;
    float t = -ray.position.y / ray.direction.y;
    if (t < 0) return false;

    outPos->x = ray.position.x + ray.direction.x * t;
    outPos->y = 0.0f;
    outPos->z = ray.position.z + ray.direction.z * t;

    float limit = 50.0f;
    outPos->x = Clamp(outPos->x, -limit, limit);
    outPos->z = Clamp(outPos->z, -limit, limit);

    return true;
}

Color CustomColorLerp(Color c1, Color c2, float amount) {
    if (amount <= 0.0f) return c1;
    if (amount >= 1.0f) return c2;
    int iAmount = (int)(amount * 256.0f);
    int invAmount = 256 - iAmount;
    return (Color){
        (unsigned char)((c1.r * invAmount + c2.r * iAmount) >> 8),
        (unsigned char)((c1.g * invAmount + c2.g * iAmount) >> 8),
        (unsigned char)((c1.b * invAmount + c2.b * iAmount) >> 8),
        255
    };
}

// --- RESIZE CALLBACK (WEB ONLY) ---
#if defined(PLATFORM_WEB)
EM_BOOL OnWindowResize(int eventType, const EmscriptenUiEvent *uiEvent, void *userData) {
    // Sync Raylib's internal buffer size with the new browser window size
    SetWindowSize(uiEvent->windowInnerWidth, uiEvent->windowInnerHeight);
    return EM_TRUE;
}
#endif

// --- CUSTOM CAMERA LOGIC ---
void UpdateCustomCamera(void) {
    Vector2 mouseDelta = GetMouseDelta();
    
    if (isCameraFirstFrame) {
        mouseDelta = (Vector2){ 0, 0 };
        isCameraFirstFrame = false;
    }

    float sensitivity = 0.003f;
    cameraYaw   -= mouseDelta.x * sensitivity;
    cameraPitch -= mouseDelta.y * sensitivity;

    if (cameraPitch > 1.5f) cameraPitch = 1.5f;
    if (cameraPitch < -1.5f) cameraPitch = -1.5f;

    Vector3 forward = {
        sinf(cameraYaw) * cosf(cameraPitch),
        sinf(cameraPitch),
        cosf(cameraYaw) * cosf(cameraPitch)
    };
    forward = Vector3Normalize(forward);
    Vector3 right = Vector3CrossProduct(forward, (Vector3){ 0, 1, 0 });
    
    float speed = 15.0f * GetFrameTime();
    Vector3 move = { 0, 0, 0 };

    if (IsKeyDown(KEY_W)) move = Vector3Add(move, forward);
    if (IsKeyDown(KEY_S)) move = Vector3Subtract(move, forward);
    if (IsKeyDown(KEY_D)) move = Vector3Add(move, right);
    if (IsKeyDown(KEY_A)) move = Vector3Subtract(move, right);
    if (IsKeyDown(KEY_SPACE)) move.y += 1.0f;
    if (IsKeyDown(KEY_LEFT_SHIFT)) move.y -= 1.0f;

    camera.position = Vector3Add(camera.position, Vector3Scale(move, speed));
    camera.target = Vector3Add(camera.position, forward);
}

// --- MAIN LOOP FUNCTION ---
void UpdateDrawFrame(void)
{
    // --- INPUT HANDLING ---
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !IsCursorHidden() && freeCameraMode) {
        DisableCursor();
        isCameraFirstFrame = true;
    }

    if (IsKeyPressed(KEY_F)) {
        freeCameraMode = !freeCameraMode;
        if (freeCameraMode) { 
            DisableCursor(); 
            isCameraFirstFrame = true;
            isTyping = false; 
        } else {
            EnableCursor();
        }
    }

    if (freeCameraMode) {
        if (IsCursorHidden()) UpdateCustomCamera();
    }

    Vector2 mouse = GetMousePosition();
    Ray ray = GetMouseRay(mouse, camera);

    if (!freeCameraMode) {
        if (IsKeyDown(KEY_UP)) fieldLineSteps += 5;
        if (IsKeyDown(KEY_DOWN)) if ((fieldLineSteps -= 5) < 10) fieldLineSteps = 10;
        if (IsKeyPressed(KEY_RIGHT)) lineResolution++;
        if (IsKeyPressed(KEY_LEFT)) if (--lineResolution < 1) lineResolution = 1;

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            bool clickedCharge = false;

            // First, check if we clicked an EXISTING charge (to select/drag)
            for (int i = 0; i < numCharges; i++) {
                Vector2 screenPos = GetWorldToScreen(charges[i].position, camera);
                if (CheckCollisionPointCircle(mouse, screenPos, 20.0f)) {
                    selectedCharge = i;
                    isTyping = false; // Stop typing if we select a charge
                    clickedCharge = true;
                    break;
                }
            }

            // If we clicked EMPTY SPACE
            if (!clickedCharge) {
                // Ensure no charge is selected so we don't drag nothing
                selectedCharge = -1; 

                // NEW LOGIC START 
                // If we are ALREADY typing and have a value, this click means "PLACE IT"
                if (isTyping && inputLength > 0) {
                    float val = strtof(chargeInput, NULL);
                    Vector3 spawnPos;
                    // Validate position and place
                    if (val != 0.0f && GetGroundIntersection(ray, &spawnPos)) {
                        if (numCharges < MAX_CHARGES) {
                            charges[numCharges++] = (Charge){spawnPos, val};
                        }
                        // Reset AFTER placing
                        isTyping = false;
                        chargeInput[0] = '\0';
                        inputLength = 0;
                    }
                } 
                // Otherwise, this click means "START TYPING"
                else {
                    isTyping = true;
                    chargeInput[0] = '\0';
                    inputLength = 0;
                }
                // NEW LOGIC END 
            }
        }

        if (selectedCharge != -1) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                Vector3 groundPos;
                if (GetGroundIntersection(ray, &groundPos)) charges[selectedCharge].position = groundPos;
            } else selectedCharge = -1;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            int deleteIndex = -1;
            for (int i = 0; i < numCharges; i++) {
                Vector2 screenPos = GetWorldToScreen(charges[i].position, camera);
                if (CheckCollisionPointCircle(mouse, screenPos, 20.0f)) {
                    deleteIndex = i; break;
                }
            }
            if (deleteIndex != -1) {
                for (int k = deleteIndex; k < numCharges - 1; k++) charges[k] = charges[k + 1];
                numCharges--; isTyping = false;
            }
        }
    }

    if (isTyping) {
        int key;
        while ((key = GetCharPressed()) > 0) {
            // FIXED: Added braces to suppress warning and ensure logic is safe
            if (((key >= '0' && key <= '9') || key == '.' || key == '-') && inputLength < 10) {
                chargeInput[inputLength++] = (char)key;
                chargeInput[inputLength] = '\0';
            }
        }
        if (IsKeyPressed(KEY_BACKSPACE) && inputLength > 0) chargeInput[--inputLength] = '\0';
        if (IsKeyPressed(MOUSE_BUTTON_LEFT)) {
            if (numCharges < MAX_CHARGES && inputLength > 0) {
                float val = strtof(chargeInput, NULL);
                Vector3 spawnPos;
                if (val != 0.0f && GetGroundIntersection(ray, &spawnPos)) 
                    charges[numCharges++] = (Charge){spawnPos, val};
            }
            isTyping = false;
        }
        if (IsKeyPressed(KEY_ESCAPE)) isTyping = false;
    }

    // --- RENDER ---
    BeginDrawing();
    ClearBackground(BLACK);

    BeginMode3D(camera);
        DrawInfiniteGrid();

        for (int i = 0; i < numCharges; i++) {
            Color c = charges[i].value > 0 ? BLUE : RED;
            if (i == selectedCharge) c = WHITE;
            DrawSphere(charges[i].position, 0.25f, c);
            DrawSphereWires(charges[i].position, 0.35f, 8, 8, Fade(c, 0.5f));
        }

        rlDrawRenderBatchActive();      
        BeginBlendMode(BLEND_ADDITIVE);
        rlBegin(RL_LINES);

        int num_phi = 4 * lineResolution; 
        int num_theta = 3 * lineResolution; 
        float startRadius = 0.1f;

        for (int j = 0; j < numCharges; j++) {
            if (charges[j].value <= 0) continue; 

            for (int t = 1; t < num_theta; t++) {
                float theta = PI * t / num_theta;
                float sinTheta = sinf(theta);
                float cosTheta = cosf(theta);

                for (int p = 0; p < num_phi; p++) {
                    float phi = 2.0f * PI * p / num_phi;
                    
                    float x = charges[j].position.x + startRadius * sinTheta * cosf(phi);
                    float y = charges[j].position.y + startRadius * sinTheta * sinf(phi);
                    float z = charges[j].position.z + startRadius * cosTheta;

                    for (int step = 0; step < fieldLineSteps; step++) {
                        float dx = 0, dy = 0, dz = 0;
                        float minDistToNeg = 10000.0f;
                        float minDistToPos = 10000.0f;
                        bool hitSink = false;

                        for (int k = 0; k < numCharges; k++) {
                            float rx = x - charges[k].position.x;
                            float ry = y - charges[k].position.y;
                            float rz = z - charges[k].position.z;
                            float r2 = rx*rx + ry*ry + rz*rz;

                            if (r2 < 0.04f) { 
                                if (charges[k].value < 0) hitSink = true;
                            }
                            float r = sqrtf(r2);

                            if (charges[k].value > 0) {
                                if (r < minDistToPos) minDistToPos = r;
                            } else {
                                if (r < minDistToNeg) minDistToNeg = r;
                            }
                            
                            float rInv = 1.0f / r;
                            float rInv3 = rInv * rInv * rInv;
                            float s = charges[k].value * rInv3;

                            dx += s * rx;
                            dy += s * ry;
                            dz += s * rz;
                        }

                        if (hitSink) break;

                        float magSq = dx*dx + dy*dy + dz*dz;
                        if (magSq < 1e-12f) break;
                        
                        float invMag = 1.0f / sqrtf(magSq);
                        dx *= invMag; dy *= invMag; dz *= invMag;

                        Vector3 start = { x, y, z };
                        x += dx * FIELD_LINE_STEP_SIZE;
                        y += dy * FIELD_LINE_STEP_SIZE;
                        z += dz * FIELD_LINE_STEP_SIZE;
                        
                        if (x*x + y*y + z*z > 2500.0f) break;

                        float mix = minDistToPos / (minDistToPos + minDistToNeg + 0.001f);
                        mix = powf(mix, 0.7f); 

                        Color col = CustomColorLerp(BLUE, RED, mix);
                        float alpha = 1.0f;
                        if (step > fieldLineSteps - 50) alpha = (fieldLineSteps - step) / 50.0f;
                        if (minDistToNeg > 20.0f) alpha *= 0.5f;

                        rlCheckRenderBatchLimit(2);
                        Color finalCol = Fade(col, 0.6f * alpha);
                        rlColor4ub(finalCol.r, finalCol.g, finalCol.b, finalCol.a);
                        rlVertex3f(start.x, start.y, start.z);
                        rlVertex3f(x, y, z);
                    }
                }
            }
        }
        rlEnd();
        EndBlendMode();
    EndMode3D();

    // --- HUD ---
    for(int i=0; i<numCharges; i++) {
        Vector2 pos = GetWorldToScreen(charges[i].position, camera);
        if (pos.x > 0 && pos.x < GetScreenWidth() && pos.y > 0 && pos.y < GetScreenHeight()) {
            const char* text = TextFormat("%.1f", charges[i].value);
            int textW = MeasureText(text, 20);
            DrawText(text, pos.x - textW/2, pos.y - 30, 20, GREEN);
        }
    }

    DrawRectangle(10, 10, 320, 440, Fade(BLACK, 0.8f));
    DrawRectangleLines(10, 10, 320, 440, DARKGRAY);
    int yOff = 20;
    DrawText("CONTROLS:", 20, yOff, 40, BLUE); yOff += 50;
    DrawText("[F] Toggle Cam/Mouse", 20, yOff, 20, WHITE); yOff += 40;
    DrawText("L-Click to Create Charge", 20, yOff, 20, ORANGE); yOff += 40;
    DrawText("L-Click Drag: Move", 20, yOff, 20, WHITE); yOff += 40;
    DrawText("R-Click: Delete", 20, yOff, 20, WHITE); yOff += 40;
    DrawText("Arrows: Density/Length", 20, yOff, 20, YELLOW); yOff += 60;
    
    DrawText(TextFormat("Line Density: %d", lineResolution), 20, yOff, 20, LIGHTGRAY); yOff += 40;
    DrawText(TextFormat("Line Steps: %d", fieldLineSteps), 20, yOff, 20, LIGHTGRAY); yOff += 60;

    if (isTyping) {
        DrawText("ENTER VALUE:", 20, yOff, 30, GREEN);
        DrawText(TextFormat("%s_", chargeInput), 260, yOff, 30, GREEN);
    }

    EndDrawing();
}

int main(void)
{
    // Enable MSAA 4x
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(initialWidth, initialHeight, "Electric Field Simulator");

    // Initialize Camera
    camera.position = (Vector3){ 15.0f, 15.0f, 15.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    
    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    cameraPitch = asinf(forward.y);
    cameraYaw = atan2f(forward.x, forward.z);

    charges[0] = (Charge){{-8, 0, 0}, 2.0f};
    charges[1] = (Charge){{8, 0, 0}, -2.0f};
    numCharges = 2;

    DisableCursor();

#if defined(PLATFORM_WEB)
    // 1. Get the current size of the browser window (element size)
    double w = initialWidth;
    double h = initialHeight;

    // FIXED: Passed "canvas" instead of NULL to ensure the size is retrieved correctly.
    // This prevents w and h from being 0 or garbage.
    emscripten_get_element_css_size("canvas", &w, &h);
    
    // Safety check: Only resize if dimensions are valid
    if (w > 0 && h > 0) {
        SetWindowSize((int)w, (int)h);
    }

    // 2. Register the callback so resizing works later
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_FALSE, OnWindowResize);

    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    SetTargetFPS(60);
    while (!WindowShouldClose())
    {
        UpdateDrawFrame();
    }
#endif

    CloseWindow();
    return 0;
}