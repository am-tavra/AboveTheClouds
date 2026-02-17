#include "raylib.h"
#include "raymath.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// World constants
#define WORLD_WIDTH 4000
#define WORLD_HEIGHT 4000
#define PLAYER_SPEED 175.0f
#define NUM_SCAVENGE_ITEMS 18
#define NUM_PARTICLES 15
#define NUM_CITY_BUILDINGS 6
#define MAX_INVENTORY 8
#define PICKUP_RADIUS 50.0f
#define PICKUP_EFFECT_DURATION 0.3f
#define FULL_MSG_DURATION 2.0f

// Colors - Refined palette
#define SAND_BASE (Color){ 232, 213, 183, 255 }
#define SAND_LIGHT (Color){ 240, 225, 195, 255 }
#define SAND_DARK (Color){ 220, 200, 170, 255 }
#define Z_BODY (Color){ 75, 45, 35, 255 }
#define Z_HEAD (Color){ 85, 55, 45, 255 }
#define Z_ACCENT (Color){ 200, 180, 160, 255 }
#define SHADOW_COLOR (Color){ 0, 0, 0, 60 }
#define VILLAGE_COLOR (Color){ 140, 110, 90, 255 }
#define VILLAGE_ROOF (Color){ 110, 80, 60, 255 }
#define WALKWAY_COLOR (Color){ 120, 95, 75, 255 }
#define WORKBENCH_COLOR (Color){ 180, 100, 60, 255 }
#define GATE_COLOR (Color){ 90, 110, 130, 255 }
#define GATE_DARK (Color){ 70, 85, 100, 255 }
#define CITY_COLOR (Color){ 65, 85, 105, 255 }
#define CITY_DARK (Color){ 50, 65, 80, 255 }
#define WINDOW_COLOR (Color){ 200, 220, 255, 180 }

// Item type enum
typedef enum {
    ITEM_ELECTRONICS,
    ITEM_POWER,
    ITEM_OPTICS,
    ITEM_STRUCTURAL
} ItemCategory;

// Item type definition (static data)
typedef struct {
    const char *name;
    ItemCategory category;
    const char *categoryName;
    Color color;
} ItemTypeDef;

static const ItemTypeDef ITEM_TYPES[] = {
    { "Circuit Board", ITEM_ELECTRONICS, "ELECTRONICS", { 100, 130, 100, 255 } },
    { "Wire Bundle",   ITEM_ELECTRONICS, "ELECTRONICS", { 180, 100,  50, 255 } },
    { "Battery Cell",  ITEM_POWER,       "POWER",       { 140,  40,  40, 255 } },
    { "Lens Array",    ITEM_OPTICS,      "OPTICS",      { 100, 160, 200, 255 } },
    { "Metal Plating", ITEM_STRUCTURAL,  "STRUCTURAL",  { 160, 160, 170, 255 } }
};
#define NUM_ITEM_TYPES 5

// World item (exists in the world, can be picked up)
typedef struct {
    int typeIndex;      // index into ITEM_TYPES
    float condition;    // 0.3 - 0.9
    Vector2 position;
    bool active;        // true = visible, false = picked up
} WorldItem;

// Inventory item (carried by player)
typedef struct {
    int typeIndex;
    float condition;
    bool occupied;
} InventorySlot;

// Pickup visual effect
typedef struct {
    Vector2 position;
    float timer;        // counts down from PICKUP_EFFECT_DURATION to 0
    bool active;
} PickupEffect;

typedef struct {
    Vector2 position;
    Vector2 velocity;
} Particle;

typedef struct {
    Vector2 position;
    Vector2 size;
    Color color;
} GroundPatch;

typedef struct {
    int heights[NUM_CITY_BUILDINGS];
} CityBuildings;

// Function prototypes
void DrawGround(GroundPatch *patches, int patchCount);
void DrawZ(Vector2 position);
void DrawDetailedBuilding(Rectangle base, bool hasWorkbench);
void DrawVillage(void);
void DrawCityGate(CityBuildings *cityBuildings);
void DrawWorldItems(WorldItem *items, int count, Vector2 playerPos, Camera2D camera);
void DrawAtmosphere(Camera2D camera, int screenWidth, int screenHeight);
void UpdateParticles(Particle *particles, int count, float deltaTime);
void DrawParticles(Particle *particles, int count);
void DrawPickupEffect(PickupEffect *effect, Camera2D camera);
void DrawInventoryScreen(InventorySlot *inventory);
void DrawHUD(InventorySlot *inventory, int screenWidth);

int CountInventory(InventorySlot *inventory)
{
    int count = 0;
    for (int i = 0; i < MAX_INVENTORY; i++) {
        if (inventory[i].occupied) count++;
    }
    return count;
}

bool AddToInventory(InventorySlot *inventory, int typeIndex, float condition)
{
    for (int i = 0; i < MAX_INVENTORY; i++) {
        if (!inventory[i].occupied) {
            inventory[i].typeIndex = typeIndex;
            inventory[i].condition = condition;
            inventory[i].occupied = true;
            return true;
        }
    }
    return false;
}

int main(void)
{
    const int screenWidth = 1280;
    const int screenHeight = 720;

    InitWindow(screenWidth, screenHeight, "Above the Clouds");
    SetTargetFPS(60);

    // Player starting position (center of world)
    Vector2 playerPos = { WORLD_WIDTH / 2.0f, WORLD_HEIGHT / 2.0f };

    // Camera setup
    Camera2D camera = { 0 };
    camera.target = playerPos;
    camera.offset = (Vector2){ screenWidth / 2.0f, screenHeight / 2.0f };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    // Create ground patches for variation
    int patchCount = 30;
    GroundPatch patches[30];
    for (int i = 0; i < patchCount; i++) {
        patches[i].position = (Vector2){
            GetRandomValue(0, WORLD_WIDTH),
            GetRandomValue(0, WORLD_HEIGHT)
        };
        patches[i].size = (Vector2){
            GetRandomValue(200, 600),
            GetRandomValue(200, 600)
        };
        patches[i].color = (GetRandomValue(0, 1) == 0) ? SAND_LIGHT : SAND_DARK;
    }

    // Create typed world items
    WorldItem worldItems[NUM_SCAVENGE_ITEMS];
    for (int i = 0; i < NUM_SCAVENGE_ITEMS; i++) {
        float angle = GetRandomValue(0, 360) * DEG2RAD;
        float distance = GetRandomValue(100, 1200);
        worldItems[i].position = (Vector2){
            WORLD_WIDTH / 2.0f + cosf(angle) * distance,
            WORLD_HEIGHT / 2.0f + sinf(angle) * distance
        };
        worldItems[i].typeIndex = GetRandomValue(0, NUM_ITEM_TYPES - 1);
        // condition: random float in [0.3, 0.9]
        worldItems[i].condition = 0.3f + (GetRandomValue(0, 600) / 1000.0f);
        worldItems[i].active = true;
    }

    // Create floating particles
    Particle particles[NUM_PARTICLES];
    for (int i = 0; i < NUM_PARTICLES; i++) {
        particles[i].position = (Vector2){
            GetRandomValue(0, WORLD_WIDTH),
            GetRandomValue(0, WORLD_HEIGHT)
        };
        particles[i].velocity = (Vector2){
            GetRandomValue(-10, 10) * 0.5f,
            GetRandomValue(-10, 10) * 0.5f
        };
    }

    // Initialize city buildings (fix flickering bug)
    CityBuildings cityBuildings;
    for (int i = 0; i < NUM_CITY_BUILDINGS; i++) {
        cityBuildings.heights[i] = GetRandomValue(60, 120);
    }

    // Inventory
    InventorySlot inventory[MAX_INVENTORY];
    for (int i = 0; i < MAX_INVENTORY; i++) {
        inventory[i].occupied = false;
        inventory[i].typeIndex = 0;
        inventory[i].condition = 0.0f;
    }

    // Pickup effect
    PickupEffect pickupEffect = { 0 };
    pickupEffect.active = false;

    // Full-inventory message
    float fullMsgTimer = 0.0f;

    // Inventory screen toggle
    bool inventoryOpen = false;

    // Main game loop
    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();

        // Toggle inventory
        if (IsKeyPressed(KEY_TAB)) {
            inventoryOpen = !inventoryOpen;
        }
        if (IsKeyPressed(KEY_ESCAPE) && inventoryOpen) {
            inventoryOpen = false;
        }

        if (!inventoryOpen) {
            // Player movement with WASD
            Vector2 movement = { 0 };
            if (IsKeyDown(KEY_W)) movement.y -= 1;
            if (IsKeyDown(KEY_S)) movement.y += 1;
            if (IsKeyDown(KEY_A)) movement.x -= 1;
            if (IsKeyDown(KEY_D)) movement.x += 1;

            // Normalize diagonal movement
            if (movement.x != 0 || movement.y != 0) {
                float length = sqrtf(movement.x * movement.x + movement.y * movement.y);
                movement.x /= length;
                movement.y /= length;
            }

            // Apply movement
            playerPos.x += movement.x * PLAYER_SPEED * deltaTime;
            playerPos.y += movement.y * PLAYER_SPEED * deltaTime;

            // Keep player within world bounds
            if (playerPos.x < 0) playerPos.x = 0;
            if (playerPos.x > WORLD_WIDTH) playerPos.x = WORLD_WIDTH;
            if (playerPos.y < 0) playerPos.y = 0;
            if (playerPos.y > WORLD_HEIGHT) playerPos.y = WORLD_HEIGHT;

            // Smooth camera follow
            camera.target = Vector2Lerp(camera.target, playerPos, 0.1f);

            // Update particles
            UpdateParticles(particles, NUM_PARTICLES, deltaTime);

            // Pickup effect timer
            if (pickupEffect.active) {
                pickupEffect.timer -= deltaTime;
                if (pickupEffect.timer <= 0.0f) {
                    pickupEffect.active = false;
                }
            }

            // Full message timer
            if (fullMsgTimer > 0.0f) {
                fullMsgTimer -= deltaTime;
                if (fullMsgTimer < 0.0f) fullMsgTimer = 0.0f;
            }

            // E to pick up nearest active item within radius
            if (IsKeyPressed(KEY_E)) {
                for (int i = 0; i < NUM_SCAVENGE_ITEMS; i++) {
                    if (!worldItems[i].active) continue;
                    float dist = Vector2Distance(playerPos, worldItems[i].position);
                    if (dist <= PICKUP_RADIUS) {
                        int invCount = CountInventory(inventory);
                        if (invCount < MAX_INVENTORY) {
                            // Add to inventory
                            AddToInventory(inventory, worldItems[i].typeIndex,
                                           worldItems[i].condition);
                            worldItems[i].active = false;
                            // Trigger pickup effect
                            pickupEffect.position = worldItems[i].position;
                            pickupEffect.timer = PICKUP_EFFECT_DURATION;
                            pickupEffect.active = true;
                        } else {
                            // Inventory full
                            fullMsgTimer = FULL_MSG_DURATION;
                        }
                        break; // only pick up one item per press
                    }
                }
            }
        }

        // Drawing
        BeginDrawing();
        ClearBackground(SAND_BASE);

        BeginMode2D(camera);

        // Draw ground with variation
        DrawGround(patches, patchCount);

        // Draw world items
        DrawWorldItems(worldItems, NUM_SCAVENGE_ITEMS, playerPos, camera);

        // Draw village
        DrawVillage();

        // Draw city gate
        DrawCityGate(&cityBuildings);

        // Draw particles
        DrawParticles(particles, NUM_PARTICLES);

        // Draw player (Z)
        DrawZ(playerPos);

        // Draw pickup effect (in world space)
        if (pickupEffect.active) {
            DrawPickupEffect(&pickupEffect, camera);
        }

        EndMode2D();

        // Draw atmosphere overlay
        DrawAtmosphere(camera, screenWidth, screenHeight);

        // HUD: pack count
        DrawHUD(inventory, screenWidth);

        // Full inventory message
        if (fullMsgTimer > 0.0f) {
            float alpha = (fullMsgTimer > 0.3f) ? 1.0f : (fullMsgTimer / 0.3f);
            unsigned char a = (unsigned char)(alpha * 220);
            const char *msg = "Inventory full — return to workbench";
            int msgW = MeasureText(msg, 20);
            int msgX = screenWidth / 2 - msgW / 2;
            int msgY = screenHeight - 80;
            DrawRectangle(msgX - 10, msgY - 6, msgW + 20, 32, (Color){ 0, 0, 0, a });
            DrawText(msg, msgX, msgY, 20, (Color){ 255, 220, 100, a });
        }

        // Inventory screen overlay
        if (inventoryOpen) {
            DrawInventoryScreen(inventory);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}

void DrawGround(GroundPatch *patches, int patchCount)
{
    DrawRectangle(0, 0, WORLD_WIDTH, WORLD_HEIGHT, SAND_BASE);
    for (int i = 0; i < patchCount; i++) {
        DrawRectangleV(patches[i].position, patches[i].size, patches[i].color);
    }
}

void DrawZ(Vector2 position)
{
    // Shadow
    DrawEllipse((int)(position.x + 1), (int)(position.y + 20), 12, 4, SHADOW_COLOR);

    // Legs
    DrawRectangle((int)(position.x - 6), (int)(position.y + 10), 4, 12, Z_BODY);
    DrawRectangle((int)(position.x + 2), (int)(position.y + 10), 4, 12, Z_BODY);

    // Body
    DrawRectangle((int)(position.x - 7), (int)(position.y - 8), 14, 18, Z_BODY);
    DrawRectangleLines((int)(position.x - 7), (int)(position.y - 8), 14, 18,
                      (Color){ 50, 30, 20, 255 });

    // Arms
    DrawRectangle((int)(position.x - 11), (int)(position.y - 2), 4, 10, Z_BODY);
    DrawRectangle((int)(position.x + 7), (int)(position.y - 2), 4, 10, Z_BODY);

    // Head
    DrawCircle((int)position.x, (int)(position.y - 16), 9, Z_HEAD);
    DrawCircleLines((int)position.x, (int)(position.y - 16), 9,
                   (Color){ 50, 30, 20, 255 });

    // Face details
    DrawCircle((int)(position.x - 3), (int)(position.y - 17), 2,
              (Color){ 40, 40, 40, 255 });
    DrawCircle((int)(position.x + 3), (int)(position.y - 17), 2,
              (Color){ 40, 40, 40, 255 });
    DrawLine((int)(position.x - 2), (int)(position.y - 13),
            (int)(position.x + 2), (int)(position.y - 13),
            (Color){ 40, 40, 40, 255 });

    // Clothing accent
    DrawRectangle((int)(position.x - 7), (int)(position.y + 2), 14, 2, Z_ACCENT);
}

void DrawDetailedBuilding(Rectangle base, bool hasWorkbench)
{
    DrawRectangle((int)base.x + 3, (int)base.y + 3,
                 (int)base.width, (int)base.height,
                 (Color){ 0, 0, 0, 40 });

    DrawRectangleRec(base, VILLAGE_COLOR);

    DrawTriangle(
        (Vector2){ base.x, base.y },
        (Vector2){ base.x + base.width, base.y },
        (Vector2){ base.x + base.width / 2, base.y - 12 },
        VILLAGE_ROOF
    );

    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 2; col++) {
            float winX = base.x + 15 + col * 25;
            float winY = base.y + 15 + row * 20;
            DrawRectangle((int)winX, (int)winY, 8, 10,
                        (Color){ 100, 120, 140, 180 });
            DrawRectangleLines((int)winX, (int)winY, 8, 10,
                             (Color){ 80, 80, 60, 255 });
        }
    }

    DrawRectangleLinesEx(base, 2, (Color){ 100, 80, 60, 255 });

    if (hasWorkbench) {
        Rectangle bench = { base.x + 10, base.y + base.height - 25, 30, 20 };
        DrawRectangleRec(bench, WORKBENCH_COLOR);
        DrawRectangleLinesEx(bench, 1, (Color){ 140, 70, 40, 255 });
        DrawCircle((int)(bench.x + 8), (int)(bench.y + 10), 3,
                  (Color){ 160, 160, 160, 255 });
        DrawRectangle((int)(bench.x + 18), (int)(bench.y + 6), 8, 2,
                    (Color){ 140, 90, 50, 255 });
    }
}

void DrawVillage(void)
{
    Vector2 villageCenter = { WORLD_WIDTH / 2.0f, WORLD_HEIGHT / 2.0f };

    Rectangle building1 = { villageCenter.x - 100, villageCenter.y - 80, 80, 60 };
    DrawDetailedBuilding(building1, true);

    Rectangle building2 = { villageCenter.x + 40, villageCenter.y - 60, 70, 50 };
    DrawDetailedBuilding(building2, false);

    Rectangle building3 = { villageCenter.x - 80, villageCenter.y + 40, 60, 55 };
    DrawDetailedBuilding(building3, false);

    Rectangle building4 = { villageCenter.x + 50, villageCenter.y + 50, 65, 50 };
    DrawDetailedBuilding(building4, false);

    DrawLineEx(
        (Vector2){ building1.x + building1.width, building1.y + building1.height / 2 },
        (Vector2){ building2.x, building2.y + building2.height / 2 },
        4, WALKWAY_COLOR
    );
    DrawLineEx(
        (Vector2){ building1.x + building1.width / 2, building1.y + building1.height },
        (Vector2){ building3.x + building3.width / 2, building3.y },
        4, WALKWAY_COLOR
    );
    DrawLineEx(
        (Vector2){ building2.x + building2.width / 2, building2.y + building2.height },
        (Vector2){ building4.x + building4.width / 2, building4.y },
        4, WALKWAY_COLOR
    );
    DrawLineEx(
        (Vector2){ building3.x + building3.width, building3.y + building3.height / 2 },
        (Vector2){ building4.x, building4.y + building4.height / 2 },
        4, WALKWAY_COLOR
    );
}

void DrawCityGate(CityBuildings *cityBuildings)
{
    Vector2 villageCenter = { WORLD_WIDTH / 2.0f, WORLD_HEIGHT / 2.0f };
    Vector2 gatePos = { villageCenter.x + 200, villageCenter.y };

    for (int i = 0; i < NUM_CITY_BUILDINGS; i++) {
        int buildingX = (int)(gatePos.x + 80 + i * 25);
        int buildingHeight = cityBuildings->heights[i];
        int buildingY = (int)(gatePos.y + 20 - buildingHeight);

        DrawRectangle(buildingX + 2, buildingY + 2, 15, buildingHeight,
                     (Color){ 0, 0, 0, 50 });
        DrawRectangle(buildingX, buildingY, 15, buildingHeight, CITY_COLOR);
        DrawRectangle(buildingX + 12, buildingY, 3, buildingHeight, CITY_DARK);

        int numFloors = buildingHeight / 15;
        for (int floor = 1; floor < numFloors; floor++) {
            int winY = buildingY + floor * 15 - 8;
            DrawRectangle(buildingX + 2, winY, 4, 6, WINDOW_COLOR);
            DrawRectangle(buildingX + 9, winY, 4, 6, WINDOW_COLOR);
        }

        DrawRectangleLines(buildingX, buildingY, 15, buildingHeight,
                         (Color){ 40, 50, 60, 255 });
    }

    DrawRectangle((int)(gatePos.x - 10) + 2, (int)(gatePos.y - 60) + 2,
                 20, 80, (Color){ 0, 0, 0, 50 });
    DrawRectangle((int)(gatePos.x + 50) + 2, (int)(gatePos.y - 60) + 2,
                 20, 80, (Color){ 0, 0, 0, 50 });

    DrawRectangle((int)(gatePos.x - 10), (int)(gatePos.y - 60), 20, 80, GATE_COLOR);
    DrawRectangle((int)(gatePos.x + 50), (int)(gatePos.y - 60), 20, 80, GATE_COLOR);

    DrawRectangle((int)(gatePos.x - 10) + 16, (int)(gatePos.y - 60),
                 4, 80, GATE_DARK);
    DrawRectangle((int)(gatePos.x + 50) + 16, (int)(gatePos.y - 60),
                 4, 80, GATE_DARK);

    DrawRectangle((int)(gatePos.x - 10) + 2, (int)(gatePos.y - 65) + 2,
                 80, 10, (Color){ 0, 0, 0, 50 });
    DrawRectangle((int)(gatePos.x - 10), (int)(gatePos.y - 65), 80, 10, GATE_COLOR);
    DrawRectangle((int)(gatePos.x - 10), (int)(gatePos.y - 65) + 7, 80, 3, GATE_DARK);

    DrawRectangleLines((int)(gatePos.x - 10), (int)(gatePos.y - 60),
                      20, 80, (Color){ 60, 70, 80, 255 });
    DrawRectangleLines((int)(gatePos.x + 50), (int)(gatePos.y - 60),
                      20, 80, (Color){ 60, 70, 80, 255 });
    DrawRectangleLines((int)(gatePos.x - 10), (int)(gatePos.y - 65),
                      80, 10, (Color){ 60, 70, 80, 255 });
}

// Draw world items with floating labels when Z is nearby
void DrawWorldItems(WorldItem *items, int count, Vector2 playerPos, Camera2D camera)
{
    for (int i = 0; i < count; i++) {
        if (!items[i].active) continue;

        Vector2 pos = items[i].position;
        Color color = ITEM_TYPES[items[i].typeIndex].color;
        float radius = 10.0f;

        // Shadow
        DrawEllipse((int)(pos.x + 2), (int)(pos.y + 2),
                   (int)radius + 1, (int)(radius / 2), SHADOW_COLOR);

        // Draw item shape based on type category
        switch (ITEM_TYPES[items[i].typeIndex].category) {
            case ITEM_ELECTRONICS: {
                // Flat rectangle (circuit board / wire bundle)
                DrawRectangle((int)(pos.x - radius), (int)(pos.y - radius * 0.6f),
                            (int)(radius * 2), (int)(radius * 1.2f), color);
                DrawRectangleLines((int)(pos.x - radius), (int)(pos.y - radius * 0.6f),
                                 (int)(radius * 2), (int)(radius * 1.2f),
                                 (Color){ color.r / 2, color.g / 2, color.b / 2, 255 });
                // Small circuit detail lines
                DrawLine((int)(pos.x - radius + 3), (int)pos.y,
                        (int)(pos.x + radius - 3), (int)pos.y,
                        (Color){ color.r / 2, color.g / 2, color.b / 2, 200 });
                break;
            }
            case ITEM_POWER: {
                // Cylinder-like (battery)
                DrawRectangle((int)(pos.x - radius * 0.6f), (int)(pos.y - radius),
                            (int)(radius * 1.2f), (int)(radius * 2), color);
                DrawRectangle((int)(pos.x - radius * 0.4f), (int)(pos.y - radius - 3),
                            (int)(radius * 0.8f), 4,
                            (Color){ color.r + 30, color.g + 30, color.b + 30, 255 });
                DrawRectangleLines((int)(pos.x - radius * 0.6f), (int)(pos.y - radius),
                                 (int)(radius * 1.2f), (int)(radius * 2),
                                 (Color){ color.r / 2, color.g / 2, color.b / 2, 255 });
                break;
            }
            case ITEM_OPTICS: {
                // Circle (lens)
                DrawCircleV(pos, radius, color);
                DrawCircleV(pos, radius * 0.5f,
                           (Color){ color.r + 40, color.g + 40, color.b + 40, 200 });
                DrawCircleLines((int)pos.x, (int)pos.y, (int)radius,
                               (Color){ color.r / 2, color.g / 2, color.b / 2, 255 });
                break;
            }
            case ITEM_STRUCTURAL: {
                // Diamond / rotated square (plating)
                Vector2 pts[4] = {
                    { pos.x,          pos.y - radius },
                    { pos.x + radius, pos.y          },
                    { pos.x,          pos.y + radius },
                    { pos.x - radius, pos.y          }
                };
                DrawTriangle(pts[0], pts[1], pts[2], color);
                DrawTriangle(pts[0], pts[2], pts[3], color);
                for (int j = 0; j < 4; j++) {
                    DrawLineEx(pts[j], pts[(j + 1) % 4], 1.5f,
                              (Color){ color.r / 2, color.g / 2, color.b / 2, 255 });
                }
                break;
            }
        }

        // Floating label when player is within pickup radius
        float dist = Vector2Distance(playerPos, pos);
        if (dist <= PICKUP_RADIUS) {
            // Build label: "Circuit Board 72%"
            char label[64];
            int condPct = (int)(items[i].condition * 100.0f);
            snprintf(label, sizeof(label), "%s %d%%",
                     ITEM_TYPES[items[i].typeIndex].name, condPct);

            int fontSize = 14;
            int labelW = MeasureText(label, fontSize);
            int labelX = (int)(pos.x - labelW / 2);
            int labelY = (int)(pos.y - radius - 22);

            // Dark background pill
            DrawRectangle(labelX - 4, labelY - 2, labelW + 8, fontSize + 4,
                         (Color){ 0, 0, 0, 160 });
            DrawText(label, labelX, labelY, fontSize, (Color){ 255, 255, 255, 255 });
        }
    }
}

void UpdateParticles(Particle *particles, int count, float deltaTime)
{
    for (int i = 0; i < count; i++) {
        particles[i].position.x += particles[i].velocity.x * deltaTime;
        particles[i].position.y += particles[i].velocity.y * deltaTime;

        if (particles[i].position.x < 0) particles[i].position.x = WORLD_WIDTH;
        if (particles[i].position.x > WORLD_WIDTH) particles[i].position.x = 0;
        if (particles[i].position.y < 0) particles[i].position.y = WORLD_HEIGHT;
        if (particles[i].position.y > WORLD_HEIGHT) particles[i].position.y = 0;
    }
}

void DrawParticles(Particle *particles, int count)
{
    for (int i = 0; i < count; i++) {
        DrawCircleV(particles[i].position, 2, (Color){ 255, 255, 255, 100 });
    }
}

// Draw expanding circle pickup effect (in world space, called inside BeginMode2D)
void DrawPickupEffect(PickupEffect *effect, Camera2D camera)
{
    (void)camera;
    float progress = 1.0f - (effect->timer / PICKUP_EFFECT_DURATION); // 0 -> 1
    float radius = progress * 40.0f;
    unsigned char alpha = (unsigned char)((1.0f - progress) * 200.0f);
    DrawCircleLines((int)effect->position.x, (int)effect->position.y,
                   radius, (Color){ 255, 255, 180, alpha });
}

// Draw full-screen inventory overlay (in screen space, outside BeginMode2D)
void DrawInventoryScreen(InventorySlot *inventory)
{
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    // Semi-transparent dark overlay
    DrawRectangle(0, 0, sw, sh, (Color){ 0, 0, 0, 200 });

    // Panel
    int panelW = 520;
    int panelH = 480;
    int panelX = sw / 2 - panelW / 2;
    int panelY = sh / 2 - panelH / 2;

    DrawRectangle(panelX, panelY, panelW, panelH, (Color){ 20, 20, 30, 240 });
    DrawRectangleLines(panelX, panelY, panelW, panelH, (Color){ 80, 80, 100, 255 });

    // Title
    const char *title = "INVENTORY";
    int titleW = MeasureText(title, 28);
    DrawText(title, panelX + panelW / 2 - titleW / 2, panelY + 16, 28,
             (Color){ 220, 210, 180, 255 });

    // Divider
    DrawLine(panelX + 16, panelY + 54, panelX + panelW - 16, panelY + 54,
             (Color){ 80, 80, 100, 255 });

    // Column headers
    DrawText("ITEM", panelX + 20, panelY + 62, 13, (Color){ 160, 160, 180, 255 });
    DrawText("TYPE", panelX + 220, panelY + 62, 13, (Color){ 160, 160, 180, 255 });
    DrawText("CONDITION", panelX + 340, panelY + 62, 13, (Color){ 160, 160, 180, 255 });

    int rowH = 44;
    int startY = panelY + 82;

    for (int i = 0; i < MAX_INVENTORY; i++) {
        int rowY = startY + i * rowH;

        // Row background (alternating)
        if (i % 2 == 0) {
            DrawRectangle(panelX + 8, rowY - 4, panelW - 16, rowH - 2,
                         (Color){ 30, 30, 45, 120 });
        }

        if (inventory[i].occupied) {
            const ItemTypeDef *def = &ITEM_TYPES[inventory[i].typeIndex];

            // Color swatch
            DrawRectangle(panelX + 12, rowY + 6, 12, 12, def->color);
            DrawRectangleLines(panelX + 12, rowY + 6, 12, 12,
                              (Color){ 255, 255, 255, 60 });

            // Item name
            DrawText(def->name, panelX + 30, rowY + 8, 16,
                     (Color){ 230, 230, 210, 255 });

            // Category
            DrawText(def->categoryName, panelX + 220, rowY + 8, 13,
                     (Color){ 160, 180, 160, 255 });

            // Condition bar
            float cond = inventory[i].condition;
            int barX = panelX + 340;
            int barY = rowY + 10;
            int barMaxW = 100;
            int barH = 10;
            int barFillW = (int)(cond * barMaxW);

            // Bar background
            DrawRectangle(barX, barY, barMaxW, barH, (Color){ 40, 40, 50, 255 });

            // Bar fill color based on condition
            Color barColor;
            if (cond < 0.5f) {
                barColor = (Color){ 200, 60, 60, 255 };
            } else if (cond < 0.8f) {
                barColor = (Color){ 220, 180, 40, 255 };
            } else {
                barColor = (Color){ 60, 180, 80, 255 };
            }
            DrawRectangle(barX, barY, barFillW, barH, barColor);
            DrawRectangleLines(barX, barY, barMaxW, barH, (Color){ 80, 80, 100, 255 });

            // Condition percentage
            char pctBuf[8];
            snprintf(pctBuf, sizeof(pctBuf), "%d%%", (int)(cond * 100.0f));
            DrawText(pctBuf, barX + barMaxW + 8, rowY + 7, 14,
                     (Color){ 210, 210, 200, 255 });

        } else {
            // Empty slot
            DrawText("— empty —", panelX + 30, rowY + 8, 15,
                     (Color){ 90, 90, 100, 255 });
        }
    }

    // Footer hint
    const char *hint = "[TAB] or [ESC] to close";
    int hintW = MeasureText(hint, 13);
    DrawText(hint, panelX + panelW / 2 - hintW / 2, panelY + panelH - 24, 13,
             (Color){ 100, 100, 120, 255 });
}

// HUD: top-right pack counter
void DrawHUD(InventorySlot *inventory, int screenWidth)
{
    int count = 0;
    for (int i = 0; i < MAX_INVENTORY; i++) {
        if (inventory[i].occupied) count++;
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "PACK: %d/%d", count, MAX_INVENTORY);

    int fontSize = 20;
    int textW = MeasureText(buf, fontSize);
    int padX = 12;
    int padY = 8;
    int rectX = screenWidth - textW - padX * 2 - 10;
    int rectY = 10;

    DrawRectangle(rectX, rectY, textW + padX * 2, fontSize + padY * 2,
                 (Color){ 0, 0, 0, 140 });
    DrawText(buf, rectX + padX, rectY + padY, fontSize,
             (Color){ 220, 210, 180, 255 });
}

void DrawAtmosphere(Camera2D camera, int screenWidth, int screenHeight)
{
    (void)camera;
    int fadeSize = 80;

    DrawRectangleGradientV(0, 0, screenWidth, fadeSize,
        (Color){ 232, 213, 183, 40 }, (Color){ 232, 213, 183, 0 });

    DrawRectangleGradientV(0, screenHeight - fadeSize, screenWidth, fadeSize,
        (Color){ 232, 213, 183, 0 }, (Color){ 232, 213, 183, 40 });

    DrawRectangleGradientH(0, 0, fadeSize, screenHeight,
        (Color){ 232, 213, 183, 40 }, (Color){ 232, 213, 183, 0 });

    DrawRectangleGradientH(screenWidth - fadeSize, 0, fadeSize, screenHeight,
        (Color){ 232, 213, 183, 0 }, (Color){ 232, 213, 183, 40 });
}
