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

// Day/night cycle
#define DAY_DURATION 180.0f

// Visual effect pool sizes
#define MAX_FOOTPRINTS 64
#define MAX_DUST_PUFFS 16
#define MAX_WIND_LINES 8

// Sandstorm states
typedef enum {
    STORM_CALM,
    STORM_BUILDING,
    STORM_ACTIVE,
    STORM_FADING
} StormState;

// --- New palette ---
// Ground
#define COL_SAND_BASE      (Color){ 212, 184, 150, 255 }
#define COL_SAND_CIRCLE_A  (Color){ 201, 168, 130, 255 }
#define COL_SAND_CIRCLE_B  (Color){ 219, 191, 163, 255 }
#define COL_SAND_CIRCLE_C  (Color){ 224, 201, 171, 255 }
#define COL_DUNE_LINE      (Color){ 184, 152, 106, 255 }
#define COL_HAZE           (Color){ 212, 196, 168, 255 }

// Z / Player
#define COL_Z_BODY         (Color){ 92,  61,  46, 255 }
#define COL_Z_HEAD         (Color){ 107, 76,  61, 255 }
#define COL_Z_SCARF        (Color){ 196, 100, 74, 255 }
#define COL_SHADOW         (Color){ 0,   0,   0,  64  }

// Village / buildings
#define COL_BLDG           (Color){ 160, 128, 96, 255 }
#define COL_BLDG_OUTLINE   (Color){ 128, 96,  64, 255 }
#define COL_BLDG_LAYER     (Color){ 140, 112, 84, 255 }
#define COL_BLDG_BORDER    (Color){ 180, 150, 116, 255 }
#define COL_CANOPY         (Color){ 212, 184, 150, 128 }
#define COL_BLDG_GLOW      (Color){ 255, 176, 102, 32  }
#define COL_WALKWAY        (Color){ 148, 120, 96, 255 }

// Workbench
#define COL_BENCH          (Color){ 139, 115, 85, 255 }
#define COL_BENCH_GLOW     (Color){ 255, 208, 112, 32  }

// City gate
#define COL_GATE_PILLAR    (Color){ 74,  85, 104, 255 }
#define COL_GATE_BAR       (Color){ 55,  65,  81, 255 }
#define COL_GATE_LIGHT     (Color){ 96, 165, 250, 255 }
#define COL_CITY_A         (Color){ 55,  65,  81, 255 }
#define COL_CITY_B         (Color){ 45,  55,  72, 255 }
#define COL_CITY_C         (Color){ 26,  32,  44, 255 }

// UI
#define COL_UI_BG          (Color){ 26,  26,  46, 224 }
#define COL_UI_BORDER      (Color){ 212, 165, 116, 255 }
#define COL_UI_TEXT        (Color){ 232, 224, 216, 255 }
#define COL_UI_HEADER      (Color){ 212, 165, 116, 255 }
#define COL_UI_DIM         (Color){ 140, 130, 120, 255 }
#define COL_UI_DARK        (Color){ 26,  26,  26,  255 }
#define COL_ALMOST_WHITE   (Color){ 240, 235, 224, 255 }
#define COL_DIVIDER        (Color){ 255, 255, 255, 21  }

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
    { "Circuit Board", ITEM_ELECTRONICS, "ELECTRONICS", { 107, 123, 107, 255 } },
    { "Wire Bundle",   ITEM_ELECTRONICS, "ELECTRONICS", { 184, 115,  51, 255 } },
    { "Battery Cell",  ITEM_POWER,       "POWER",       { 139,  58,  58, 255 } },
    { "Lens Array",    ITEM_OPTICS,      "OPTICS",      { 135, 206, 235, 255 } },
    { "Metal Plating", ITEM_STRUCTURAL,  "STRUCTURAL",  { 168, 168, 168, 255 } }
};
#define NUM_ITEM_TYPES 5

// World item (exists in the world, can be picked up)
typedef struct {
    int typeIndex;      // index into ITEM_TYPES
    float condition;    // 0.3 - 0.9
    Vector2 position;
    bool active;        // true = visible, false = picked up
    float respawnTimer; // countdown in seconds; > 0 means waiting to respawn
} WorldItem;

// Spawn shimmer effect (appears when an item respawns)
typedef struct {
    Vector2 position;
    float timer;        // counts down from 1.0 to 0
    bool active;
} SpawnShimmer;

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

// Ground soft circle (stored once at startup)
typedef struct {
    Vector2 center;
    float radius;
    Color color;
} GroundCircle;

// Dune line: stored as a series of points approximating a sweeping arc
#define DUNE_SEGMENTS 12
typedef struct {
    Vector2 pts[DUNE_SEGMENTS + 1];
    int numPts;
    float width;
} DuneLine;

typedef struct {
    int heights[NUM_CITY_BUILDINGS];
} CityBuildings;

// Static initialized arrays
#define NUM_GROUND_CIRCLES 18
#define NUM_DUNE_LINES 6

// --- Visual enhancement structs ---

// Footprint
typedef struct {
    Vector2 position;
    float alpha;
    float timer;
    bool active;
} Footprint;

// Dust puff
typedef struct {
    Vector2 position;
    float timer;
    float maxTimer;
    float radius;
    bool active;
} DustPuff;

// Wind line (screen space)
typedef struct {
    float y;
    float x;
    float speed;
    float alpha;
    float length;
    bool active;
} WindLine;

// Parallax dune silhouette (static, initialized once)
#define NUM_PARALLAX_DUNES 4
typedef struct {
    float x;
    float y;
    float width;
    float height;
    float topLeftOffsetX;   // trapezoid shape variation
    float topRightOffsetX;
} ParallaxDune;

// Sandstorm particle (screen space)
#define MAX_STORM_PARTICLES 60
typedef struct {
    float x;
    float y;
    float speed;
    float alpha;
    float length;
    float size;
} StormParticle;

// Time-of-day palette
typedef struct {
    Color skyColor;
    Color groundTint;
    unsigned char ambientAlpha;
} TODPalette;

// Function prototypes
void DrawGround(GroundCircle *circles, int circleCount,
                DuneLine *dunes, int duneCount);
void DrawZ(Vector2 position, float walkTimer, float breathTimer,
           Vector2 facing, float shadowOffsetX, float shadowOffsetY);
void DrawDetailedBuilding(Rectangle base, bool hasWorkbench, float pulseTimer,
                          int buildingIndex, bool isNight, float shadowOffsetX, float shadowOffsetY);
void DrawVillage(float pulseTimer, bool isNight, float shadowOffsetX, float shadowOffsetY);
void DrawCityGate(CityBuildings *cityBuildings, float pulseTimer, bool isNight);
void DrawWorldItems(WorldItem *items, int count, Vector2 playerPos,
                    Camera2D camera, float pulseTimer,
                    float shadowOffsetX, float shadowOffsetY, bool isNight);
void DrawAtmosphere(Camera2D camera, int screenWidth, int screenHeight);
void UpdateParticles(Particle *particles, int count, float deltaTime);
void DrawParticles(Particle *particles, int count);
void DrawPickupEffect(PickupEffect *effect, Camera2D camera);
void DrawInventoryScreen(InventorySlot *inventory);
void DrawHUD(InventorySlot *inventory, int screenWidth);
void DrawParallaxDunes(ParallaxDune *dunes, int count, Camera2D camera,
                       int screenWidth, int screenHeight);
void DrawFootprints(Footprint *footprints, int count);
void DrawDustPuffs(DustPuff *puffs, int count);
void DrawWindLines(WindLine *lines, int count);
void DrawHeatShimmer(Camera2D camera, int screenWidth, int screenHeight, float pulseTimer);
void DrawDayNightOverlay(float dayPhase, int screenWidth, int screenHeight);
void DrawSunMoon(float dayPhase, int screenWidth);
void DrawStormOverlay(StormState state, float stormPhase, StormParticle *particles,
                      int count, int screenWidth, int screenHeight);
void DrawSpawnShimmers(SpawnShimmer *shimmers, int count);

// Helper: draw a rounded rectangle using a rectangle + circles at corners
static void DrawRoundRect(float x, float y, float w, float h, float r, Color col)
{
    // Center fill
    DrawRectangle((int)(x + r), (int)y, (int)(w - 2*r), (int)h, col);
    DrawRectangle((int)x, (int)(y + r), (int)w, (int)(h - 2*r), col);
    // Corner circles
    DrawCircle((int)(x + r),     (int)(y + r),     r, col);
    DrawCircle((int)(x + w - r), (int)(y + r),     r, col);
    DrawCircle((int)(x + r),     (int)(y + h - r), r, col);
    DrawCircle((int)(x + w - r), (int)(y + h - r), r, col);
}

// Helper: color lerp
static Color ColorLerpRGBA(Color a, Color b, float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        (unsigned char)(a.a + (b.a - a.a) * t)
    };
}

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

    // --- Initialize ground circles (stored once, no flickering) ---
    GroundCircle groundCircles[NUM_GROUND_CIRCLES];
    Color circleColors[3] = { COL_SAND_CIRCLE_A, COL_SAND_CIRCLE_B, COL_SAND_CIRCLE_C };
    for (int i = 0; i < NUM_GROUND_CIRCLES; i++) {
        groundCircles[i].center = (Vector2){
            (float)GetRandomValue(0, WORLD_WIDTH),
            (float)GetRandomValue(0, WORLD_HEIGHT)
        };
        groundCircles[i].radius = (float)GetRandomValue(120, 320);
        Color c = circleColors[i % 3];
        // Alpha 30-40
        int alpha = GetRandomValue(30, 40);
        groundCircles[i].color = (Color){ c.r, c.g, c.b, (unsigned char)alpha };
    }

    // --- Initialize dune lines (stored once) ---
    DuneLine duneLines[NUM_DUNE_LINES];
    for (int d = 0; d < NUM_DUNE_LINES; d++) {
        // Random arc: pick a center, radius, and angular sweep
        float cx   = (float)GetRandomValue(200, WORLD_WIDTH  - 200);
        float cy   = (float)GetRandomValue(200, WORLD_HEIGHT - 200);
        float arcR = (float)GetRandomValue(300, 700);
        float startA = (float)GetRandomValue(0, 180) * DEG2RAD;
        float sweepA = (float)GetRandomValue(60, 140) * DEG2RAD;

        int numPts = DUNE_SEGMENTS + 1;
        for (int s = 0; s < numPts; s++) {
            float t = (float)s / (float)DUNE_SEGMENTS;
            float angle = startA + t * sweepA;
            duneLines[d].pts[s].x = cx + cosf(angle) * arcR;
            duneLines[d].pts[s].y = cy + sinf(angle) * arcR;
        }
        duneLines[d].numPts = numPts;
        duneLines[d].width  = (float)GetRandomValue(2, 3);
    }

    // Create typed world items
    WorldItem worldItems[NUM_SCAVENGE_ITEMS];
    for (int i = 0; i < NUM_SCAVENGE_ITEMS; i++) {
        float angle    = GetRandomValue(0, 360) * DEG2RAD;
        float distance = (float)GetRandomValue(100, 1200);
        worldItems[i].position = (Vector2){
            WORLD_WIDTH  / 2.0f + cosf(angle) * distance,
            WORLD_HEIGHT / 2.0f + sinf(angle) * distance
        };
        worldItems[i].typeIndex     = GetRandomValue(0, NUM_ITEM_TYPES - 1);
        worldItems[i].condition     = 0.3f + (GetRandomValue(0, 600) / 1000.0f);
        worldItems[i].active        = true;
        worldItems[i].respawnTimer  = 0.0f;
    }

    // Create floating particles (drift left-to-right at varying speeds)
    Particle particles[NUM_PARTICLES];
    for (int i = 0; i < NUM_PARTICLES; i++) {
        particles[i].position = (Vector2){
            (float)GetRandomValue(0, WORLD_WIDTH),
            (float)GetRandomValue(0, WORLD_HEIGHT)
        };
        // Primarily left-to-right drift, small vertical wander
        float speed = (float)GetRandomValue(8, 25);
        particles[i].velocity = (Vector2){
            speed,
            (float)GetRandomValue(-4, 4)
        };
    }

    // Initialize city buildings (fix flickering bug - stored once)
    CityBuildings cityBuildings;
    for (int i = 0; i < NUM_CITY_BUILDINGS; i++) {
        cityBuildings.heights[i] = GetRandomValue(60, 120);
    }

    // Inventory
    InventorySlot inventory[MAX_INVENTORY];
    for (int i = 0; i < MAX_INVENTORY; i++) {
        inventory[i].occupied  = false;
        inventory[i].typeIndex = 0;
        inventory[i].condition = 0.0f;
    }

    // Pickup effect
    PickupEffect pickupEffect = { 0 };
    pickupEffect.active = false;

    // Spawn shimmer pool (one slot per item)
    SpawnShimmer spawnShimmers[NUM_SCAVENGE_ITEMS];
    for (int i = 0; i < NUM_SCAVENGE_ITEMS; i++) {
        spawnShimmers[i].active = false;
        spawnShimmers[i].timer  = 0.0f;
    }

    // Full-inventory message
    float fullMsgTimer = 0.0f;

    // Inventory screen toggle
    bool inventoryOpen = false;

    // Animation timers
    float walkTimer   = 0.0f;   // increments when moving, used for walk bob
    float breathTimer = 0.0f;   // always increments, used for idle breathing (3s cycle)
    float pulseTimer  = 0.0f;   // always increments, used for workbench/gate pulses

    // --- Day/night cycle ---
    float dayTimer = 45.0f;     // start at roughly "noon-ish"
    float dayPhase = dayTimer / DAY_DURATION;

    // --- Pickup flash ---
    float pickupFlashTimer = 0.0f;
    float pickupFlashMax   = 0.2f;

    // --- Directional facing ---
    Vector2 facing = { 0.0f, 1.0f };   // default facing down
    Vector2 prevMovement = { 0.0f, 0.0f };

    // --- Footprints ---
    Footprint footprints[MAX_FOOTPRINTS];
    for (int i = 0; i < MAX_FOOTPRINTS; i++) footprints[i].active = false;
    int footprintHead = 0;
    Vector2 lastFootprintPos = playerPos;

    // --- Dust puffs ---
    DustPuff dustPuffs[MAX_DUST_PUFFS];
    for (int i = 0; i < MAX_DUST_PUFFS; i++) dustPuffs[i].active = false;
    int dustPuffHead = 0;
    float dustTimer = 0.0f;
    bool wasMoving = false;

    // --- Wind lines ---
    WindLine windLines[MAX_WIND_LINES];
    for (int i = 0; i < MAX_WIND_LINES; i++) windLines[i].active = false;
    float windSpawnTimer = 0.0f;
    float windSpawnInterval = (float)GetRandomValue(200, 800) / 100.0f;

    // --- Parallax dunes (initialized once) ---
    ParallaxDune parallaxDunes[NUM_PARALLAX_DUNES];
    for (int i = 0; i < NUM_PARALLAX_DUNES; i++) {
        parallaxDunes[i].x              = (float)GetRandomValue(-200, screenWidth);
        parallaxDunes[i].y              = (float)GetRandomValue(20, 120);
        parallaxDunes[i].width          = (float)GetRandomValue(300, 600);
        parallaxDunes[i].height         = (float)GetRandomValue(30, 70);
        parallaxDunes[i].topLeftOffsetX  = (float)GetRandomValue(40, 100);
        parallaxDunes[i].topRightOffsetX = (float)GetRandomValue(40, 100);
    }

    // --- Sandstorm system ---
    StormState stormState = STORM_CALM;
    float stormTimer     = (float)GetRandomValue(6000, 12000) / 100.0f; // 60-120s until first storm
    float stormDuration  = 0.0f;
    float stormPhase     = 0.0f;   // 0..1 progress through current state
    float stormMsgAlpha  = 0.0f;
    float stormSpeedMult = 1.0f;

    StormParticle stormParticles[MAX_STORM_PARTICLES];
    for (int i = 0; i < MAX_STORM_PARTICLES; i++) {
        stormParticles[i].x      = (float)GetRandomValue(0, screenWidth);
        stormParticles[i].y      = (float)GetRandomValue(0, screenHeight);
        stormParticles[i].speed  = (float)GetRandomValue(300, 700);
        stormParticles[i].alpha  = (float)GetRandomValue(60, 120);
        stormParticles[i].length = (float)GetRandomValue(20, 80);
        stormParticles[i].size   = (float)GetRandomValue(10, 30) / 10.0f;
    }

    // Main game loop
    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();

        // Always advance breath and pulse timers
        breathTimer += deltaTime;
        pulseTimer  += deltaTime;

        // --- Day/night cycle ---
        dayTimer += deltaTime;
        if (dayTimer >= DAY_DURATION) dayTimer -= DAY_DURATION;
        dayPhase = dayTimer / DAY_DURATION;
        bool isNight = (dayPhase > 0.75f || dayPhase < 0.05f);

        // Dynamic shadow offsets (rotate through the day)
        float shadowOffsetX = cosf(dayPhase * 2.0f * PI) * 8.0f;
        float shadowOffsetY = 6.0f;
        if (isNight) { shadowOffsetX = 0.0f; shadowOffsetY = 0.0f; }

        // Toggle inventory
        if (IsKeyPressed(KEY_TAB)) {
            inventoryOpen = !inventoryOpen;
        }
        if (IsKeyPressed(KEY_ESCAPE) && inventoryOpen) {
            inventoryOpen = false;
        }

        // --- Sandstorm state machine ---
        stormTimer -= deltaTime;
        if (stormState == STORM_CALM && stormTimer <= 0.0f) {
            stormState    = STORM_BUILDING;
            stormDuration = 5.0f;
            stormTimer    = stormDuration;
            stormPhase    = 0.0f;
            printf("SANDSTORM building...\n");
        } else if (stormState == STORM_BUILDING) {
            stormPhase = 1.0f - (stormTimer / stormDuration);
            stormMsgAlpha = stormPhase;
            if (stormTimer <= 0.0f) {
                stormState    = STORM_ACTIVE;
                stormDuration = (float)GetRandomValue(2000, 3000) / 100.0f;
                stormTimer    = stormDuration;
                stormPhase    = 0.0f;
                stormSpeedMult = 0.7f;
                printf("SANDSTORM\n");
            }
        } else if (stormState == STORM_ACTIVE) {
            stormPhase = stormTimer / stormDuration;
            stormMsgAlpha = 0.0f;
            if (stormTimer <= 0.0f) {
                stormState    = STORM_FADING;
                stormDuration = 5.0f;
                stormTimer    = stormDuration;
                stormPhase    = 0.0f;
            }
        } else if (stormState == STORM_FADING) {
            stormPhase = stormTimer / stormDuration;
            stormSpeedMult = 0.7f + (1.0f - stormPhase) * 0.3f;
            if (stormTimer <= 0.0f) {
                stormState    = STORM_CALM;
                stormTimer    = (float)GetRandomValue(6000, 12000) / 100.0f;
                stormPhase    = 0.0f;
                stormSpeedMult = 1.0f;
                stormMsgAlpha  = 0.0f;
            }
        }

        if (!inventoryOpen) {
            // Player movement with WASD
            Vector2 movement = { 0 };
            if (IsKeyDown(KEY_W)) movement.y -= 1;
            if (IsKeyDown(KEY_S)) movement.y += 1;
            if (IsKeyDown(KEY_A)) movement.x -= 1;
            if (IsKeyDown(KEY_D)) movement.x += 1;

            bool isMoving = (movement.x != 0 || movement.y != 0);

            // Normalize diagonal movement
            if (isMoving) {
                float length = sqrtf(movement.x * movement.x + movement.y * movement.y);
                movement.x /= length;
                movement.y /= length;
                walkTimer += deltaTime * 6.0f;  // speed_factor = 6 -> ~1Hz bob at normal speed

                // Track facing direction
                facing.x = movement.x;
                facing.y = movement.y;

                // Dust puffs: on start of movement or direction change
                bool dirChanged = (movement.x != prevMovement.x || movement.y != prevMovement.y);
                if (!wasMoving || dirChanged) {
                    // Spawn a bigger dust puff
                    DustPuff *dp = &dustPuffs[dustPuffHead % MAX_DUST_PUFFS];
                    dp->position = playerPos;
                    dp->timer    = 0.4f;
                    dp->maxTimer = 0.4f;
                    dp->radius   = 12.0f;
                    dp->active   = true;
                    dustPuffHead++;
                }

                // Continuous small dust puffs while moving
                dustTimer += deltaTime;
                if (dustTimer >= 0.15f) {
                    dustTimer = 0.0f;
                    DustPuff *dp = &dustPuffs[dustPuffHead % MAX_DUST_PUFFS];
                    dp->position = playerPos;
                    dp->timer    = 0.3f;
                    dp->maxTimer = 0.3f;
                    dp->radius   = 6.0f;
                    dp->active   = true;
                    dustPuffHead++;
                }

                // Footprints: when moved more than 15px from last footprint
                float distFromLast = Vector2Distance(playerPos, lastFootprintPos);
                if (distFromLast >= 15.0f) {
                    Footprint *fp = &footprints[footprintHead % MAX_FOOTPRINTS];
                    fp->position = playerPos;
                    fp->alpha    = 120.0f;
                    fp->timer    = 4.0f;
                    fp->active   = true;
                    footprintHead++;
                    lastFootprintPos = playerPos;
                }
            } else {
                dustTimer = 0.0f;
            }

            wasMoving     = isMoving;
            prevMovement  = movement;

            // Apply movement (with storm speed multiplier)
            float effectiveSpeed = PLAYER_SPEED * stormSpeedMult;
            playerPos.x += movement.x * effectiveSpeed * deltaTime;
            playerPos.y += movement.y * effectiveSpeed * deltaTime;

            // Keep player within world bounds
            if (playerPos.x < 0)            playerPos.x = 0;
            if (playerPos.x > WORLD_WIDTH)  playerPos.x = WORLD_WIDTH;
            if (playerPos.y < 0)            playerPos.y = 0;
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

            // Pickup flash timer
            if (pickupFlashTimer > 0.0f) {
                pickupFlashTimer -= deltaTime;
                if (pickupFlashTimer < 0.0f) pickupFlashTimer = 0.0f;
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
                            AddToInventory(inventory, worldItems[i].typeIndex,
                                           worldItems[i].condition);
                            worldItems[i].active = false;
                            worldItems[i].respawnTimer = 60.0f + (float)GetRandomValue(0, 30);
                            pickupEffect.position = worldItems[i].position;
                            pickupEffect.timer    = PICKUP_EFFECT_DURATION;
                            pickupEffect.active   = true;
                            pickupFlashTimer      = pickupFlashMax;
                        } else {
                            fullMsgTimer = FULL_MSG_DURATION;
                        }
                        break; // only pick up one item per press
                    }
                }
            }
        }

        // --- Update footprints ---
        for (int i = 0; i < MAX_FOOTPRINTS; i++) {
            if (!footprints[i].active) continue;
            footprints[i].timer -= deltaTime;
            footprints[i].alpha = (footprints[i].timer / 4.0f) * 120.0f;
            if (footprints[i].timer <= 0.0f) {
                footprints[i].active = false;
            }
        }

        // --- Update dust puffs ---
        for (int i = 0; i < MAX_DUST_PUFFS; i++) {
            if (!dustPuffs[i].active) continue;
            dustPuffs[i].timer -= deltaTime;
            if (dustPuffs[i].timer <= 0.0f) {
                dustPuffs[i].active = false;
            }
        }

        // --- Update item respawn timers ---
        for (int i = 0; i < NUM_SCAVENGE_ITEMS; i++) {
            if (worldItems[i].active) continue;
            if (worldItems[i].respawnTimer <= 0.0f) continue;
            worldItems[i].respawnTimer -= deltaTime;
            if (worldItems[i].respawnTimer <= 0.0f) {
                worldItems[i].respawnTimer = 0.0f;
                // Pick a new position outside the village (>200px from world center)
                float wx, wy;
                float cx = WORLD_WIDTH  / 2.0f;
                float cy = WORLD_HEIGHT / 2.0f;
                do {
                    wx = 100.0f + (float)GetRandomValue(0, WORLD_WIDTH  - 200);
                    wy = 100.0f + (float)GetRandomValue(0, WORLD_HEIGHT - 200);
                } while (fabsf(wx - cx) < 200.0f && fabsf(wy - cy) < 200.0f);
                worldItems[i].position  = (Vector2){ wx, wy };
                worldItems[i].typeIndex = GetRandomValue(0, NUM_ITEM_TYPES - 1);
                worldItems[i].condition = 0.3f + (float)GetRandomValue(0, 600) / 1000.0f;
                worldItems[i].active    = true;
                // Trigger shimmer at new position (reuse slot i)
                spawnShimmers[i].position = worldItems[i].position;
                spawnShimmers[i].timer    = 1.0f;
                spawnShimmers[i].active   = true;
            }
        }

        // --- Update spawn shimmer timers ---
        for (int i = 0; i < NUM_SCAVENGE_ITEMS; i++) {
            if (!spawnShimmers[i].active) continue;
            spawnShimmers[i].timer -= deltaTime;
            if (spawnShimmers[i].timer <= 0.0f) {
                spawnShimmers[i].timer  = 0.0f;
                spawnShimmers[i].active = false;
            }
        }

        // --- Update wind lines ---
        windSpawnTimer += deltaTime;
        // During storm building: double spawn frequency
        float effectiveWindInterval = windSpawnInterval;
        if (stormState == STORM_BUILDING || stormState == STORM_ACTIVE) {
            effectiveWindInterval *= 0.5f;
        }
        if (windSpawnTimer >= effectiveWindInterval) {
            windSpawnTimer = 0.0f;
            windSpawnInterval = (float)GetRandomValue(200, 800) / 100.0f;
            // Find inactive slot
            for (int i = 0; i < MAX_WIND_LINES; i++) {
                if (!windLines[i].active) {
                    windLines[i].y      = (float)GetRandomValue(0, screenHeight);
                    windLines[i].x      = (float)screenWidth + 10.0f;
                    windLines[i].speed  = (float)GetRandomValue(400, 800);
                    windLines[i].alpha  = 60.0f;
                    windLines[i].length = (float)GetRandomValue(60, 200);
                    windLines[i].active = true;
                    break;
                }
            }
        }
        for (int i = 0; i < MAX_WIND_LINES; i++) {
            if (!windLines[i].active) continue;
            windLines[i].x -= windLines[i].speed * deltaTime;
            // Fade alpha based on horizontal position
            float progress = 1.0f - ((windLines[i].x + windLines[i].length) / (float)(screenWidth + windLines[i].length + 10));
            windLines[i].alpha = 60.0f * (1.0f - progress);
            if (windLines[i].x + windLines[i].length < 0) {
                windLines[i].active = false;
            }
        }

        // --- Update storm particles ---
        if (stormState == STORM_ACTIVE || stormState == STORM_BUILDING || stormState == STORM_FADING) {
            for (int i = 0; i < MAX_STORM_PARTICLES; i++) {
                stormParticles[i].x -= stormParticles[i].speed * deltaTime;
                if (stormParticles[i].x + stormParticles[i].length < 0) {
                    stormParticles[i].x = (float)(screenWidth + 10);
                    stormParticles[i].y = (float)GetRandomValue(0, screenHeight);
                }
            }
        }

        // Drawing
        BeginDrawing();
        ClearBackground(COL_SAND_BASE);

        // Draw parallax background BEFORE BeginMode2D (screen space with parallax offset)
        DrawParallaxDunes(parallaxDunes, NUM_PARALLAX_DUNES, camera, screenWidth, screenHeight);

        BeginMode2D(camera);

        // Draw ground
        DrawGround(groundCircles, NUM_GROUND_CIRCLES, duneLines, NUM_DUNE_LINES);

        // Draw footprints (above ground, below Z)
        DrawFootprints(footprints, MAX_FOOTPRINTS);

        // Draw spawn shimmers (above ground, below items)
        DrawSpawnShimmers(spawnShimmers, NUM_SCAVENGE_ITEMS);

        // Draw world items
        DrawWorldItems(worldItems, NUM_SCAVENGE_ITEMS, playerPos, camera, pulseTimer,
                       shadowOffsetX, shadowOffsetY, isNight);

        // Draw village (contains workbench)
        DrawVillage(pulseTimer, isNight, shadowOffsetX, shadowOffsetY);

        // Draw city gate
        DrawCityGate(&cityBuildings, pulseTimer, isNight);

        // Draw particles (in world space)
        DrawParticles(particles, NUM_PARTICLES);

        // Draw dust puffs (in world space, below Z)
        DrawDustPuffs(dustPuffs, MAX_DUST_PUFFS);

        // Draw player (Z)
        DrawZ(playerPos, walkTimer, breathTimer, facing, shadowOffsetX, shadowOffsetY);

        // Draw pickup effect (in world space)
        if (pickupEffect.active) {
            DrawPickupEffect(&pickupEffect, camera);
        }

        // Heat shimmer at world edges
        DrawHeatShimmer(camera, screenWidth, screenHeight, pulseTimer);

        EndMode2D();

        // --- Day/night overlay ---
        DrawDayNightOverlay(dayPhase, screenWidth, screenHeight);

        // Draw atmosphere overlay (screen space)
        DrawAtmosphere(camera, screenWidth, screenHeight);

        // Draw wind lines (screen space)
        DrawWindLines(windLines, MAX_WIND_LINES);

        // Draw storm overlay (screen space)
        DrawStormOverlay(stormState, stormPhase, stormParticles, MAX_STORM_PARTICLES,
                         screenWidth, screenHeight);

        // Pickup flash (after EndMode2D, before EndDrawing)
        if (pickupFlashTimer > 0.0f) {
            float t = pickupFlashTimer / pickupFlashMax;
            unsigned char flashA = (unsigned char)(t * 40.0f);
            DrawRectangle(0, 0, screenWidth, screenHeight, (Color){ 255, 240, 200, flashA });
        }

        // Sun/moon indicator
        DrawSunMoon(dayPhase, screenWidth);

        // Storm "wind picking up" hint
        if (stormState == STORM_BUILDING && stormMsgAlpha > 0.0f) {
            unsigned char ma = (unsigned char)(stormMsgAlpha * 180.0f);
            const char *stormMsg = "wind picking up...";
            int smW = MeasureText(stormMsg, 16);
            int smX = screenWidth / 2 - smW / 2;
            DrawText(stormMsg, smX, 56, 16, (Color){ 212, 184, 150, ma });
        }

        // HUD: pack count
        DrawHUD(inventory, screenWidth);

        // Full inventory message
        if (fullMsgTimer > 0.0f) {
            float alpha = (fullMsgTimer > 0.3f) ? 1.0f : (fullMsgTimer / 0.3f);
            unsigned char a  = (unsigned char)(alpha * 220);
            const char *msg  = "Inventory full - return to workbench";
            int msgW = MeasureText(msg, 20);
            int msgX = screenWidth / 2 - msgW / 2;
            int msgY = screenHeight - 80;
            DrawRectangle(msgX - 12, msgY - 6, msgW + 24, 32, (Color){ 26, 26, 46, a });
            DrawRectangleLines(msgX - 12, msgY - 6, msgW + 24, 32,
                               (Color){ 212, 165, 116, a });
            DrawText(msg, msgX, msgY, 20, (Color){ 212, 165, 116, a });
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

// ---------------------------------------------------------------------------
// DrawGround
// ---------------------------------------------------------------------------
void DrawGround(GroundCircle *circles, int circleCount,
                DuneLine *dunes, int duneCount)
{
    // Base fill
    DrawRectangle(0, 0, WORLD_WIDTH, WORLD_HEIGHT, COL_SAND_BASE);

    // Large soft circles blended at low alpha
    for (int i = 0; i < circleCount; i++) {
        DrawCircleV(circles[i].center, circles[i].radius, circles[i].color);
    }

    // Near-edge haze: fade ground toward haze color along each border
    int fadeW = 300;
    Color hazeOpaque = { 212, 196, 168, 220 };
    Color hazeClear  = { 212, 196, 168, 0   };
    DrawRectangleGradientH(0, 0, fadeW, WORLD_HEIGHT,             hazeOpaque, hazeClear);
    DrawRectangleGradientH(WORLD_WIDTH - fadeW, 0, fadeW, WORLD_HEIGHT, hazeClear, hazeOpaque);
    DrawRectangleGradientV(0, 0, WORLD_WIDTH, fadeW,              hazeOpaque, hazeClear);
    DrawRectangleGradientV(0, WORLD_HEIGHT - fadeW, WORLD_WIDTH, fadeW, hazeClear, hazeOpaque);

    // Curved dune lines
    for (int d = 0; d < duneCount; d++) {
        for (int s = 0; s < dunes[d].numPts - 1; s++) {
            DrawLineEx(dunes[d].pts[s], dunes[d].pts[s + 1],
                       dunes[d].width, COL_DUNE_LINE);
        }
    }
}

// ---------------------------------------------------------------------------
// DrawFootprints
// ---------------------------------------------------------------------------
void DrawFootprints(Footprint *footprints, int count)
{
    for (int i = 0; i < count; i++) {
        if (!footprints[i].active) continue;
        unsigned char a = (unsigned char)(footprints[i].alpha);
        if (a == 0) continue;
        Color fpColor = { 160, 130, 100, a };
        float px = footprints[i].position.x;
        float py = footprints[i].position.y;
        // Two small ellipses: left foot and right foot
        DrawEllipse((int)(px - 4), (int)(py + 2), 3, 2, fpColor);
        DrawEllipse((int)(px + 4), (int)(py + 2), 3, 2, fpColor);
    }
}

// ---------------------------------------------------------------------------
// DrawDustPuffs
// ---------------------------------------------------------------------------
void DrawDustPuffs(DustPuff *puffs, int count)
{
    for (int i = 0; i < count; i++) {
        if (!puffs[i].active) continue;
        float progress = 1.0f - (puffs[i].timer / puffs[i].maxTimer);
        float radius   = 2.0f + progress * (puffs[i].radius - 2.0f);
        float alphaF   = (1.0f - progress) * 80.0f;
        if (alphaF < 1.0f) continue;
        unsigned char a = (unsigned char)alphaF;
        Color puffColor = { 212, 196, 168, a };
        DrawCircleLines((int)puffs[i].position.x, (int)puffs[i].position.y,
                        radius, puffColor);
    }
}

// ---------------------------------------------------------------------------
// DrawWindLines  (screen space)
// ---------------------------------------------------------------------------
void DrawWindLines(WindLine *lines, int count)
{
    for (int i = 0; i < count; i++) {
        if (!lines[i].active) continue;
        if (lines[i].alpha < 1.0f) continue;
        unsigned char a = (unsigned char)lines[i].alpha;
        Color wlColor = { 212, 184, 150, a };
        DrawLineEx(
            (Vector2){ lines[i].x, lines[i].y },
            (Vector2){ lines[i].x + lines[i].length, lines[i].y },
            1.5f, wlColor
        );
    }
}

// ---------------------------------------------------------------------------
// DrawParallaxDunes  (screen space, before BeginMode2D)
// ---------------------------------------------------------------------------
void DrawParallaxDunes(ParallaxDune *dunes, int count, Camera2D camera,
                       int screenWidth, int screenHeight)
{
    (void)screenWidth;
    // Parallax offset at 0.3x camera speed
    float parallaxX = camera.target.x * 0.3f - camera.offset.x * 0.3f;
    float parallaxY = camera.target.y * 0.3f - camera.offset.y * 0.3f;

    Color duneColor = { 180, 160, 140, 160 };

    for (int i = 0; i < count; i++) {
        float bx = dunes[i].x - parallaxX;
        float by = dunes[i].y - parallaxY * 0.1f; // minimal vertical parallax
        float bw = dunes[i].width;
        float bh = dunes[i].height;

        // Clamp to near the top of screen + reasonable band
        float screenBy = by;
        if (screenBy < 0) screenBy = 0;
        if (screenBy > screenHeight * 0.4f) screenBy = (float)(screenHeight) * 0.4f;

        // Draw as a trapezoid using two triangles
        float tl = dunes[i].topLeftOffsetX;
        float tr = dunes[i].topRightOffsetX;
        Vector2 bottomLeft  = { bx,       screenBy + bh };
        Vector2 bottomRight = { bx + bw,  screenBy + bh };
        Vector2 topLeft     = { bx + tl,  screenBy      };
        Vector2 topRight    = { bx + bw - tr, screenBy  };

        // Only draw if top is to the left of bottom-right and makes sense
        if (topLeft.x < topRight.x) {
            DrawTriangle(topLeft, topRight, bottomRight, duneColor);
            DrawTriangle(topLeft, bottomRight, bottomLeft, duneColor);
        }
    }
}

// ---------------------------------------------------------------------------
// DrawHeatShimmer (world space edges)
// ---------------------------------------------------------------------------
void DrawHeatShimmer(Camera2D camera, int screenWidth, int screenHeight, float pulseTimer)
{
    // Visible world area
    float visLeft   = camera.target.x - camera.offset.x;
    float visTop    = camera.target.y - camera.offset.y;
    float visRight  = visLeft + screenWidth;
    float visBottom = visTop  + screenHeight;

    Color shimmerColor = { 212, 196, 168, 30 };
    float edgeDist = 400.0f;
    int numLines = 20;
    float spacing = 15.0f;

    // Left edge shimmer
    if (visLeft < edgeDist) {
        for (int li = 0; li < numLines; li++) {
            float worldY = visTop + li * spacing;
            float waveY  = worldY + sinf(pulseTimer * 3.0f + visLeft * 0.05f) * 2.0f;
            float lineLen = (float)GetRandomValue(20, 40);
            DrawLineEx(
                (Vector2){ visLeft + 10.0f, waveY },
                (Vector2){ visLeft + 10.0f + lineLen, waveY },
                1.0f, shimmerColor
            );
        }
    }
    // Right edge shimmer
    if (visRight > WORLD_WIDTH - edgeDist) {
        for (int li = 0; li < numLines; li++) {
            float worldY = visTop + li * spacing;
            float waveY  = worldY + sinf(pulseTimer * 3.0f + visRight * 0.05f) * 2.0f;
            float lineLen = (float)GetRandomValue(20, 40);
            DrawLineEx(
                (Vector2){ visRight - 10.0f - lineLen, waveY },
                (Vector2){ visRight - 10.0f, waveY },
                1.0f, shimmerColor
            );
        }
    }
    // Top edge shimmer
    if (visTop < edgeDist) {
        for (int li = 0; li < numLines; li++) {
            float worldX = visLeft + li * spacing;
            float waveX  = worldX + sinf(pulseTimer * 3.0f + worldX * 0.05f) * 2.0f;
            float lineLen = (float)GetRandomValue(20, 40);
            DrawLineEx(
                (Vector2){ waveX, visTop + 10.0f },
                (Vector2){ waveX, visTop + 10.0f + lineLen },
                1.0f, shimmerColor
            );
        }
    }
    // Bottom edge shimmer
    if (visBottom > WORLD_HEIGHT - edgeDist) {
        for (int li = 0; li < numLines; li++) {
            float worldX = visLeft + li * spacing;
            float waveX  = worldX + sinf(pulseTimer * 3.0f + worldX * 0.05f) * 2.0f;
            float lineLen = (float)GetRandomValue(20, 40);
            DrawLineEx(
                (Vector2){ waveX, visBottom - 10.0f - lineLen },
                (Vector2){ waveX, visBottom - 10.0f },
                1.0f, shimmerColor
            );
        }
    }
}

// ---------------------------------------------------------------------------
// DrawDayNightOverlay  (screen space)
// ---------------------------------------------------------------------------
void DrawDayNightOverlay(float dayPhase, int screenWidth, int screenHeight)
{
    // 4 time-of-day palettes:
    // Index 0: Dawn  (0.0)
    // Index 1: Noon  (0.25)
    // Index 2: Golden hour (0.6)
    // Index 3: Night (0.85)
    typedef struct { float phase; Color tint; unsigned char ambAlpha; } TODKey;
    static const TODKey keys[] = {
        { 0.00f, { 255, 200, 150, 255 }, 180 },
        { 0.25f, { 232, 220, 200, 255 },   0 },
        { 0.60f, { 255, 160,  80, 255 }, 120 },
        { 0.85f, {  40,  50,  80, 255 }, 200 },
        { 1.00f, { 255, 200, 150, 255 }, 180 }  // wraps back to dawn
    };
    int numKeys = 5;

    // Find which segment we're in
    int seg = 0;
    for (int k = 0; k < numKeys - 1; k++) {
        if (dayPhase >= keys[k].phase && dayPhase < keys[k+1].phase) {
            seg = k;
            break;
        }
    }
    float segLen = keys[seg+1].phase - keys[seg].phase;
    float t = (segLen > 0.0f) ? ((dayPhase - keys[seg].phase) / segLen) : 0.0f;

    Color tintA = keys[seg].tint;
    Color tintB = keys[seg+1].tint;
    unsigned char alphaA = keys[seg].ambAlpha;
    unsigned char alphaB = keys[seg+1].ambAlpha;

    unsigned char finalAlpha = (unsigned char)(alphaA + (alphaB - alphaA) * t);
    Color finalTint = ColorLerpRGBA(tintA, tintB, t);
    finalTint.a = finalAlpha;

    if (finalAlpha > 0) {
        DrawRectangle(0, 0, screenWidth, screenHeight, finalTint);
    }
}

// ---------------------------------------------------------------------------
// DrawSunMoon  (screen space, top-left arc indicator)
// ---------------------------------------------------------------------------
void DrawSunMoon(float dayPhase, int screenWidth)
{
    (void)screenWidth;
    // Arc: goes from left (0.0) to right to left again over the day
    // dayPhase 0..1 -> angle along arc
    float arcRadius = 50.0f;
    float arcCenterX = 80.0f;
    float arcCenterY = 80.0f;
    // angle: phase 0 = left (PI), phase 0.5 = right (0), phase 1 = left again
    float angle = PI - dayPhase * 2.0f * PI;
    float bodyX = arcCenterX + cosf(angle) * arcRadius;
    float bodyY = arcCenterY - sinf(angle) * arcRadius * 0.5f; // flatten arc

    bool isNightPhase = (dayPhase > 0.75f || dayPhase < 0.05f);
    Color bodyColor = isNightPhase ? (Color){ 224, 224, 255, 220 } : (Color){ 255, 215, 0, 220 };

    // Draw faint arc track
    for (int ai = 0; ai <= 20; ai++) {
        float ta = PI - ((float)ai / 20.0f) * 2.0f * PI;
        float ax = arcCenterX + cosf(ta) * arcRadius;
        float ay = arcCenterY - sinf(ta) * arcRadius * 0.5f;
        DrawCircle((int)ax, (int)ay, 1, (Color){ 255, 255, 255, 20 });
    }

    DrawCircle((int)bodyX, (int)bodyY, 8, bodyColor);

    // Moon crescent detail at night
    if (isNightPhase) {
        DrawCircle((int)(bodyX + 3), (int)bodyY, 7, (Color){ 40, 50, 80, 200 });
    }
}

// ---------------------------------------------------------------------------
// DrawStormOverlay  (screen space)
// ---------------------------------------------------------------------------
void DrawStormOverlay(StormState state, float stormPhase, StormParticle *particles,
                      int count, int screenWidth, int screenHeight)
{
    if (state == STORM_CALM) return;

    float intensity = 0.0f;
    if (state == STORM_BUILDING)  intensity = stormPhase;
    else if (state == STORM_ACTIVE)   intensity = 1.0f;
    else if (state == STORM_FADING)   intensity = stormPhase;

    if (intensity <= 0.0f) return;

    // Screen dust tint
    unsigned char tintA = (unsigned char)(intensity * 80.0f);
    DrawRectangle(0, 0, screenWidth, screenHeight, (Color){ 180, 150, 100, tintA });

    // Storm particles
    int activeCount = (int)(intensity * (float)count);
    for (int i = 0; i < activeCount && i < count; i++) {
        unsigned char a = (unsigned char)(particles[i].alpha * intensity);
        Color pColor = { 200, 170, 130, a };
        DrawLineEx(
            (Vector2){ particles[i].x, particles[i].y },
            (Vector2){ particles[i].x + particles[i].length, particles[i].y },
            particles[i].size, pColor
        );
    }

    // Stronger vignette during storm
    unsigned char vigA = (unsigned char)(intensity * 60.0f);
    Color vigColor = { 20, 15, 10, vigA };
    Color vigClear = { 20, 15, 10, 0   };
    int fadeSize = 150;
    DrawRectangleGradientV(0, 0, screenWidth, fadeSize, vigColor, vigClear);
    DrawRectangleGradientV(0, screenHeight - fadeSize, screenWidth, fadeSize, vigClear, vigColor);
    DrawRectangleGradientH(0, 0, fadeSize, screenHeight, vigColor, vigClear);
    DrawRectangleGradientH(screenWidth - fadeSize, 0, fadeSize, screenHeight, vigClear, vigColor);
}

// ---------------------------------------------------------------------------
// DrawZ  (player character with walk bob, idle breathing, facing direction, legs)
// ---------------------------------------------------------------------------
void DrawZ(Vector2 position, float walkTimer, float breathTimer,
           Vector2 facing, float shadowOffsetX, float shadowOffsetY)
{
    // Walk bob: +-1-2px vertical, synced to walk cycle
    float bob = sinf(walkTimer) * 1.5f;

    // Breathing: body grows 1px taller on 3s cycle when idle
    float breathScale = sinf(breathTimer * (2.0f * PI / 3.0f)) * 0.5f + 0.5f; // 0..1
    int breathPx = (int)(breathScale * 1.0f); // 0 or 1 extra px

    float px = position.x;
    float py = position.y + bob;

    // Directional body width
    bool facingLeft  = (facing.x < -0.3f);
    bool facingUp    = (facing.y < -0.3f && fabsf(facing.x) < 0.7f);
    bool facingDown  = (facing.y >  0.3f && fabsf(facing.x) < 0.7f);
    int bodyW = 12;
    if (facingUp)   bodyW = 10;
    if (facingDown) bodyW = 14;

    // Shadow offset: offset behind travel direction
    float sx = shadowOffsetX != 0.0f ? shadowOffsetX : 4.0f;
    float sy = shadowOffsetY != 0.0f ? shadowOffsetY : 6.0f;
    // Additional directional shadow offset (behind Z)
    sx -= facing.x * 4.0f;
    sy -= facing.y * 4.0f;
    // Clamp shadow offset
    if (shadowOffsetX == 0.0f && shadowOffsetY == 0.0f) {
        // Night: no shadow
    } else {
        DrawEllipse((int)(px + sx), (int)(py + sy + 16), 18, 5, COL_SHADOW);
    }

    // --- Legs ---
    float legOffset = sinf(walkTimer * 8.0f) * 4.0f;
    int legW = 4;
    int legH = 8;
    // Left leg
    DrawRectangle(
        (int)(px - bodyW/2),
        (int)(py + 10 - legH/2 + legOffset),
        legW, legH, COL_Z_BODY
    );
    // Right leg
    DrawRectangle(
        (int)(px + bodyW/2 - legW),
        (int)(py + 10 - legH/2 - legOffset),
        legW, legH, COL_Z_BODY
    );

    // Body: rounded rect (+ breath)
    int bodyX = (int)(px - bodyW / 2);
    int bodyY = (int)(py - 10);
    int bodyH = 20 + breathPx;
    DrawRoundRect((float)bodyX, (float)bodyY, (float)bodyW, (float)bodyH, 3.0f, COL_Z_BODY);

    // Scarf: small triangle - mirror to left side when facing left
    float scarfBaseY = py - 2.0f;
    if (facingLeft) {
        Vector2 scarfA = { px - 6,       scarfBaseY - 5 };
        Vector2 scarfB = { px - 6,       scarfBaseY + 5 };
        Vector2 scarfC = { px - 12,      scarfBaseY     };
        DrawTriangle(scarfB, scarfA, scarfC, COL_Z_SCARF);
    } else {
        Vector2 scarfA = { px + 6,       scarfBaseY - 5 };
        Vector2 scarfB = { px + 6,       scarfBaseY + 5 };
        Vector2 scarfC = { px + 12,      scarfBaseY     };
        DrawTriangle(scarfA, scarfB, scarfC, COL_Z_SCARF);
    }

    // Head: circle 8px radius, slightly lighter
    float headY = py - 18.0f;
    DrawCircle((int)px, (int)headY, 8, COL_Z_HEAD);

    // Face: two small eye dots
    DrawCircle((int)(px - 3), (int)(headY - 1), 1, (Color){ 40, 26, 20, 255 });
    DrawCircle((int)(px + 3), (int)(headY - 1), 1, (Color){ 40, 26, 20, 255 });
}

// ---------------------------------------------------------------------------
// DrawDetailedBuilding
// ---------------------------------------------------------------------------
void DrawDetailedBuilding(Rectangle base, bool hasWorkbench, float pulseTimer,
                          int buildingIndex, bool isNight,
                          float shadowOffsetX, float shadowOffsetY)
{
    // Building shadow
    if (shadowOffsetX != 0.0f || shadowOffsetY != 0.0f) {
        DrawRectangle(
            (int)(base.x + shadowOffsetX * 1.5f),
            (int)(base.y + shadowOffsetY * 1.5f),
            (int)base.width, (int)base.height,
            (Color){ 0, 0, 0, 32 }
        );
    }

    // Sandy border (slightly lighter than building, darker than ground)
    DrawRectangle((int)(base.x - 4), (int)(base.y - 4),
                  (int)(base.width + 8), (int)(base.height + 8), COL_BLDG_BORDER);

    // Building body
    DrawRectangleRec(base, COL_BLDG);
    DrawRectangleLinesEx(base, 2.0f, COL_BLDG_OUTLINE);

    // Horizontal construction layer lines inside building
    int numLayers = 3;
    for (int l = 1; l <= numLayers; l++) {
        float lineY = base.y + (base.height / (float)(numLayers + 1)) * l;
        DrawLineEx(
            (Vector2){ base.x + 4, lineY },
            (Vector2){ base.x + base.width - 4, lineY },
            1.0f, COL_BLDG_LAYER
        );
    }

    // Animated shade cloth canopy: undulate with sin
    float poleH   = 18.0f;
    float spreadX = base.width * 0.55f;
    // Ripple offset: +-3px vertically, different phase per building
    float ripple = sinf(pulseTimer * 1.5f + buildingIndex * 0.7f) * 3.0f;

    Vector2 leftBase  = { base.x + base.width * 0.25f,  base.y };
    Vector2 rightBase = { base.x + base.width * 0.75f,  base.y };
    Vector2 leftTop   = { base.x + base.width * 0.25f - spreadX, base.y - poleH + ripple };
    Vector2 rightTop  = { base.x + base.width * 0.75f + spreadX, base.y - poleH - ripple };

    // Draw canopy rectangle (filled quad via two triangles)
    DrawTriangle(leftTop, rightTop, rightBase, COL_CANOPY);
    DrawTriangle(leftTop, rightBase, leftBase,  COL_CANOPY);

    // Pole lines
    DrawLineEx(leftBase,  leftTop,  1.5f, COL_BLDG_OUTLINE);
    DrawLineEx(rightBase, rightTop, 1.5f, COL_BLDG_OUTLINE);

    // Warm glow at entrance - brighter at night
    float entrX = base.x + base.width  / 2.0f;
    float entrY = base.y + base.height;
    unsigned char glowEntrA = isNight ? 60 : 32;
    DrawCircle((int)entrX, (int)entrY, 30, (Color){ 255, 176, 102, glowEntrA });

    // Workbench inside first building
    if (hasWorkbench) {
        // Pulsing glow: radius oscillates 30..40 on 2s cycle
        float pulseFactor = sinf(pulseTimer * PI) * 0.5f + 0.5f; // 0..1
        float glowR = 30.0f + pulseFactor * 10.0f;
        unsigned char glowA = (unsigned char)(28 + pulseFactor * 8);
        Color glowColor = { COL_BENCH_GLOW.r, COL_BENCH_GLOW.g,
                            COL_BENCH_GLOW.b, glowA };

        float benchCX = base.x + base.width  / 2.0f;
        float benchCY = base.y + base.height - 18.0f;
        DrawCircle((int)benchCX, (int)benchCY, (int)glowR, glowColor);

        // Bench rectangle (wider)
        Rectangle bench = {
            base.x + base.width / 2.0f - 22,
            base.y + base.height - 28.0f,
            44, 20
        };
        DrawRectangleRec(bench, COL_BENCH);
        DrawRectangleLinesEx(bench, 1.5f, COL_BLDG_OUTLINE);

        // Tiny colored component dots on bench surface
        Color dotColors[4] = {
            { 107, 123, 107, 255 },  // green-gray (circuit)
            { 184, 115,  51, 255 },  // copper (wire)
            { 139,  58,  58, 255 },  // dark red (battery)
            { 135, 206, 235, 255 }   // light blue (lens)
        };
        for (int d = 0; d < 4; d++) {
            float dotX = bench.x + 5 + d * 10;
            float dotY = bench.y + 6;
            DrawCircle((int)dotX, (int)dotY, 3, dotColors[d]);
        }

        // Wrench icon above bench: two small rectangles at angle
        float wrenchX = bench.x + bench.width / 2.0f;
        float wrenchY = bench.y - 10.0f;
        // Handle (vertical-ish)
        DrawRectanglePro(
            (Rectangle){ wrenchX, wrenchY, 3, 10 },
            (Vector2){ 1.5f, 5.0f },
            -20.0f,
            (Color){ 200, 190, 170, 255 }
        );
        // Head (horizontal-ish)
        DrawRectanglePro(
            (Rectangle){ wrenchX, wrenchY, 8, 3 },
            (Vector2){ 4.0f, 1.5f },
            -20.0f,
            (Color){ 200, 190, 170, 255 }
        );
    }
}

// ---------------------------------------------------------------------------
// DrawVillage
// ---------------------------------------------------------------------------
void DrawVillage(float pulseTimer, bool isNight, float shadowOffsetX, float shadowOffsetY)
{
    Vector2 villageCenter = { WORLD_WIDTH / 2.0f, WORLD_HEIGHT / 2.0f };

    Rectangle building1 = { villageCenter.x - 100, villageCenter.y - 80, 80, 60 };
    Rectangle building2 = { villageCenter.x + 40,  villageCenter.y - 60, 70, 50 };
    Rectangle building3 = { villageCenter.x - 80,  villageCenter.y + 40, 60, 55 };
    Rectangle building4 = { villageCenter.x + 50,  villageCenter.y + 50, 65, 50 };

    DrawDetailedBuilding(building1, true,  pulseTimer, 0, isNight, shadowOffsetX, shadowOffsetY);
    DrawDetailedBuilding(building2, false, pulseTimer, 1, isNight, shadowOffsetX, shadowOffsetY);
    DrawDetailedBuilding(building3, false, pulseTimer, 2, isNight, shadowOffsetX, shadowOffsetY);
    DrawDetailedBuilding(building4, false, pulseTimer, 3, isNight, shadowOffsetX, shadowOffsetY);

    // Walkways connecting buildings
    DrawLineEx(
        (Vector2){ building1.x + building1.width, building1.y + building1.height / 2 },
        (Vector2){ building2.x,                   building2.y + building2.height / 2 },
        4, COL_WALKWAY
    );
    DrawLineEx(
        (Vector2){ building1.x + building1.width / 2, building1.y + building1.height },
        (Vector2){ building3.x + building3.width / 2, building3.y },
        4, COL_WALKWAY
    );
    DrawLineEx(
        (Vector2){ building2.x + building2.width / 2, building2.y + building2.height },
        (Vector2){ building4.x + building4.width / 2, building4.y },
        4, COL_WALKWAY
    );
    DrawLineEx(
        (Vector2){ building3.x + building3.width,     building3.y + building3.height / 2 },
        (Vector2){ building4.x,                       building4.y + building4.height / 2 },
        4, COL_WALKWAY
    );
}

// ---------------------------------------------------------------------------
// DrawCityGate
// ---------------------------------------------------------------------------
void DrawCityGate(CityBuildings *cityBuildings, float pulseTimer, bool isNight)
{
    Vector2 villageCenter = { WORLD_WIDTH / 2.0f, WORLD_HEIGHT / 2.0f };
    Vector2 gatePos       = { villageCenter.x + 200, villageCenter.y };

    // City background buildings at varying heights
    Color cityColors[3] = { COL_CITY_A, COL_CITY_B, COL_CITY_C };
    for (int i = 0; i < NUM_CITY_BUILDINGS; i++) {
        int bx = (int)(gatePos.x + 80 + i * 28);
        int bh = cityBuildings->heights[i];
        int by = (int)(gatePos.y + 20 - bh);
        int bw = 18;

        Color bc = cityColors[i % 3];
        DrawRectangle(bx, by, bw, bh, bc);
        DrawRectangleLines(bx, by, bw, bh, (Color){ 26, 32, 44, 255 });
    }

    // Gate pillar dimensions
    int pillarW = 20;
    int pillarH = 80;
    int leftPillarX  = (int)(gatePos.x - 10);
    int rightPillarX = (int)(gatePos.x + 50);
    int pillarY      = (int)(gatePos.y - 60);

    // Pillars (tall, narrow)
    DrawRectangle(leftPillarX,  pillarY, pillarW, pillarH, COL_GATE_PILLAR);
    DrawRectangle(rightPillarX, pillarY, pillarW, pillarH, COL_GATE_PILLAR);
    DrawRectangleLines(leftPillarX,  pillarY, pillarW, pillarH,
                       (Color){ 40, 50, 64, 255 });
    DrawRectangleLines(rightPillarX, pillarY, pillarW, pillarH,
                       (Color){ 40, 50, 64, 255 });

    // Horizontal bar at top
    int barY = pillarY - 10;
    DrawRectangle(leftPillarX, barY, rightPillarX - leftPillarX + pillarW, 10,
                  COL_GATE_BAR);

    // Pulsing blue light stripe - brighter at night
    float lightPulse = sinf(pulseTimer * (2.0f * PI / 1.5f)) * 0.5f + 0.5f; // 0..1
    unsigned char lightA;
    if (isNight) {
        lightA = (unsigned char)(80 + lightPulse * 80.0f);
    } else {
        lightA = (unsigned char)(40 + lightPulse * 40.0f);
    }
    Color lightColor = { COL_GATE_LIGHT.r, COL_GATE_LIGHT.g,
                         COL_GATE_LIGHT.b, lightA };

    // Thin 4px stripe vertically centered on each pillar
    int stripeW = 4;
    int stripeX_left  = leftPillarX  + (pillarW - stripeW) / 2;
    int stripeX_right = rightPillarX + (pillarW - stripeW) / 2;
    DrawRectangle(stripeX_left,  pillarY, stripeW, pillarH, lightColor);
    DrawRectangle(stripeX_right, pillarY, stripeW, pillarH, lightColor);

    // Thin horizontal blue line at gate base
    Color baseLineColor = { COL_GATE_LIGHT.r, COL_GATE_LIGHT.g,
                            COL_GATE_LIGHT.b, 64 };
    DrawLineEx(
        (Vector2){ (float)leftPillarX,               (float)(pillarY + pillarH) },
        (Vector2){ (float)(rightPillarX + pillarW),  (float)(pillarY + pillarH) },
        1.5f, baseLineColor
    );
}

// ---------------------------------------------------------------------------
// DrawWorldItems
// ---------------------------------------------------------------------------
void DrawWorldItems(WorldItem *items, int count, Vector2 playerPos,
                    Camera2D camera, float pulseTimer,
                    float shadowOffsetX, float shadowOffsetY, bool isNight)
{
    (void)camera;
    (void)isNight;

    for (int i = 0; i < count; i++) {
        if (!items[i].active) continue;

        Vector2 pos       = items[i].position;
        int     typeIdx   = items[i].typeIndex;
        Color   itemColor = ITEM_TYPES[typeIdx].color;
        float   dist      = Vector2Distance(playerPos, pos);
        bool    inRange   = (dist <= PICKUP_RADIUS);

        // Per-item pulsing glow phase offset
        float phase     = pulseTimer * 2.0f + (float)typeIdx;
        float pulseFact = sinf(phase) * 0.5f + 0.5f; // 0..1

        // Glow circle: intensifies when in range
        unsigned char glowA;
        if (inRange) {
            glowA = (unsigned char)(50 + pulseFact * 10.0f);
        } else {
            glowA = (unsigned char)(18 + pulseFact * 7.0f);
        }
        Color glowColor = { itemColor.r, itemColor.g, itemColor.b, glowA };
        DrawCircle((int)pos.x, (int)pos.y, 22, glowColor);

        // Dynamic shadow
        if (shadowOffsetX != 0.0f || shadowOffsetY != 0.0f) {
            DrawEllipse((int)(pos.x + shadowOffsetX), (int)(pos.y + shadowOffsetY), 12, 4, COL_SHADOW);
        } else {
            DrawEllipse((int)(pos.x + 2), (int)(pos.y + 2), 12, 4, COL_SHADOW);
        }

        // Draw item shape by type index
        switch (typeIdx) {
            case 0: {
                // Circuit Board: rectangle with grid lines, green-gray
                int rx = (int)(pos.x - 9), ry = (int)(pos.y - 6);
                int rw = 18, rh = 12;
                DrawRectangle(rx, ry, rw, rh, itemColor);
                DrawRectangleLines(rx, ry, rw, rh,
                    (Color){ itemColor.r / 2, itemColor.g / 2, itemColor.b / 2, 255 });
                // Grid lines
                for (int g = 1; g <= 2; g++) {
                    DrawLine(rx + g * 6, ry, rx + g * 6, ry + rh,
                        (Color){ itemColor.r / 2, itemColor.g / 2, itemColor.b / 2, 160 });
                }
                DrawLine(rx, ry + 6, rx + rw, ry + 6,
                    (Color){ itemColor.r / 2, itemColor.g / 2, itemColor.b / 2, 160 });
                break;
            }
            case 1: {
                // Wire Bundle: 3 short angled rectangles (copper)
                float angles[3] = { -25.0f, 0.0f, 25.0f };
                for (int w = 0; w < 3; w++) {
                    DrawRectanglePro(
                        (Rectangle){ pos.x - 1, pos.y - 8, 3, 16 },
                        (Vector2){ 1.5f, 8.0f },
                        angles[w],
                        itemColor
                    );
                }
                break;
            }
            case 2: {
                // Battery Cell: rectangle with nub on top, dark red
                int bx = (int)(pos.x - 7), by = (int)(pos.y - 9);
                int bw = 14, bh = 18;
                DrawRectangle(bx, by, bw, bh, itemColor);
                DrawRectangleLines(bx, by, bw, bh,
                    (Color){ itemColor.r / 2, itemColor.g / 2, itemColor.b / 2, 255 });
                // Nub on top
                DrawRectangle((int)(pos.x - 3), by - 4, 6, 4,
                    (Color){ itemColor.r + 30 > 255 ? 255 : itemColor.r + 30,
                             itemColor.g + 30 > 255 ? 255 : itemColor.g + 30,
                             itemColor.b + 30 > 255 ? 255 : itemColor.b + 30, 255 });
                break;
            }
            case 3: {
                // Lens Array: circle with dot in center, light blue
                DrawCircle((int)pos.x, (int)pos.y, 10, itemColor);
                DrawCircleLines((int)pos.x, (int)pos.y, 10,
                    (Color){ itemColor.r / 2, itemColor.g / 2, itemColor.b / 2, 255 });
                DrawCircle((int)pos.x, (int)pos.y, 3,
                    (Color){ itemColor.r / 2, itemColor.g / 2, itemColor.b / 2, 255 });
                break;
            }
            case 4: {
                // Metal Plating: diamond/rhombus (rotated square), silver
                float r = 10.0f;
                Vector2 pts[4] = {
                    { pos.x,     pos.y - r },
                    { pos.x + r, pos.y     },
                    { pos.x,     pos.y + r },
                    { pos.x - r, pos.y     }
                };
                DrawTriangle(pts[0], pts[1], pts[2], itemColor);
                DrawTriangle(pts[0], pts[2], pts[3], itemColor);
                for (int e = 0; e < 4; e++) {
                    DrawLineEx(pts[e], pts[(e + 1) % 4], 1.5f,
                        (Color){ itemColor.r / 2, itemColor.g / 2, itemColor.b / 2, 255 });
                }
                break;
            }
            default:
                DrawCircleV(pos, 10.0f, itemColor);
                break;
        }

        // Floating label when player is within pickup radius
        if (inRange) {
            char label[64];
            int condPct = (int)(items[i].condition * 100.0f);
            snprintf(label, sizeof(label), "%s %d%%",
                     ITEM_TYPES[typeIdx].name, condPct);

            int fontSize = 14;
            int labelW   = MeasureText(label, fontSize);
            int labelX   = (int)(pos.x - labelW / 2);
            int labelY   = (int)(pos.y - 12 - 22);

            DrawRectangle(labelX - 6, labelY - 3, labelW + 12, fontSize + 6,
                         COL_UI_BG);
            DrawRectangleLines(labelX - 6, labelY - 3, labelW + 12, fontSize + 6,
                              COL_UI_BORDER);
            DrawText(label, labelX, labelY, fontSize, COL_UI_TEXT);
        }
    }
}

// ---------------------------------------------------------------------------
// UpdateParticles
// ---------------------------------------------------------------------------
void UpdateParticles(Particle *particles, int count, float deltaTime)
{
    for (int i = 0; i < count; i++) {
        particles[i].position.x += particles[i].velocity.x * deltaTime;
        particles[i].position.y += particles[i].velocity.y * deltaTime;

        // Wrap around world
        if (particles[i].position.x < 0)            particles[i].position.x = WORLD_WIDTH;
        if (particles[i].position.x > WORLD_WIDTH)  particles[i].position.x = 0;
        if (particles[i].position.y < 0)             particles[i].position.y = WORLD_HEIGHT;
        if (particles[i].position.y > WORLD_HEIGHT)  particles[i].position.y = 0;
    }
}

// ---------------------------------------------------------------------------
// DrawParticles
// ---------------------------------------------------------------------------
void DrawParticles(Particle *particles, int count)
{
    Color particleColor = { 212, 196, 168, 96 };
    for (int i = 0; i < count; i++) {
        // Alternate 1px and 2px dots
        int r = (i % 2 == 0) ? 1 : 2;
        DrawCircle((int)particles[i].position.x, (int)particles[i].position.y,
                   r, particleColor);
    }
}

// ---------------------------------------------------------------------------
// DrawPickupEffect
// ---------------------------------------------------------------------------
void DrawPickupEffect(PickupEffect *effect, Camera2D camera)
{
    (void)camera;
    float progress = 1.0f - (effect->timer / PICKUP_EFFECT_DURATION);
    float radius   = progress * 40.0f;
    unsigned char alpha = (unsigned char)((1.0f - progress) * 200.0f);
    DrawCircleLines((int)effect->position.x, (int)effect->position.y,
                   radius, (Color){ 212, 165, 116, alpha });
}

// ---------------------------------------------------------------------------
// DrawInventoryScreen
// ---------------------------------------------------------------------------
void DrawInventoryScreen(InventorySlot *inventory)
{
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    // Semi-transparent overlay
    DrawRectangle(0, 0, sw, sh, (Color){ 26, 26, 46, 200 });

    // Panel
    int panelW = 520;
    int panelH = 480;
    int panelX = sw / 2 - panelW / 2;
    int panelY = sh / 2 - panelH / 2;
    int pad    = 12;

    DrawRectangle(panelX, panelY, panelW, panelH, COL_UI_BG);
    DrawRectangleLines(panelX, panelY, panelW, panelH, COL_UI_BORDER);

    // Title
    const char *title = "INVENTORY";
    int titleW = MeasureText(title, 28);
    DrawText(title, panelX + panelW / 2 - titleW / 2, panelY + pad + 4, 28,
             COL_UI_HEADER);

    // Divider
    DrawLine(panelX + pad, panelY + 54,
             panelX + panelW - pad, panelY + 54, COL_UI_BORDER);

    // Column headers
    DrawText("ITEM",      panelX + pad + 8,  panelY + 62, 13, COL_UI_DIM);
    DrawText("TYPE",      panelX + 220,       panelY + 62, 13, COL_UI_DIM);
    DrawText("CONDITION", panelX + 340,       panelY + 62, 13, COL_UI_DIM);

    int rowH   = 44;
    int startY = panelY + 82;

    for (int i = 0; i < MAX_INVENTORY; i++) {
        int rowY = startY + i * rowH;

        // Entry divider line
        if (i > 0) {
            DrawLine(panelX + pad, rowY - 1,
                     panelX + panelW - pad, rowY - 1, COL_DIVIDER);
        }

        if (inventory[i].occupied) {
            const ItemTypeDef *def = &ITEM_TYPES[inventory[i].typeIndex];

            // Color swatch
            DrawRectangle(panelX + pad, rowY + 6, 12, 12, def->color);
            DrawRectangleLines(panelX + pad, rowY + 6, 12, 12,
                              (Color){ 255, 255, 255, 40 });

            // Item name
            DrawText(def->name, panelX + pad + 18, rowY + 8, 16, COL_UI_TEXT);

            // Category
            DrawText(def->categoryName, panelX + 220, rowY + 8, 13,
                     (Color){ 180, 200, 180, 255 });

            // Condition bar
            float cond      = inventory[i].condition;
            int barX        = panelX + 340;
            int barY        = rowY + 10;
            int barMaxW     = 100;
            int barH        = 10;
            int barFillW    = (int)(cond * barMaxW);

            DrawRectangle(barX, barY, barMaxW, barH, (Color){ 40, 40, 56, 255 });

            Color barColor;
            if (cond < 0.5f) {
                barColor = (Color){ 200, 60, 60, 255 };
            } else if (cond < 0.8f) {
                barColor = (Color){ 220, 180, 40, 255 };
            } else {
                barColor = (Color){ 60, 180, 80, 255 };
            }
            DrawRectangle(barX, barY, barFillW, barH, barColor);
            DrawRectangleLines(barX, barY, barMaxW, barH, COL_UI_BORDER);

            char pctBuf[8];
            snprintf(pctBuf, sizeof(pctBuf), "%d%%", (int)(cond * 100.0f));
            DrawText(pctBuf, barX + barMaxW + 8, rowY + 7, 14, COL_UI_TEXT);

        } else {
            DrawText("- empty -", panelX + pad + 8, rowY + 8, 15,
                     (Color){ 90, 90, 100, 255 });
        }
    }

    // Footer hint
    const char *hint  = "[TAB] or [ESC] to close";
    int hintW = MeasureText(hint, 13);
    DrawText(hint, panelX + panelW / 2 - hintW / 2, panelY + panelH - 24, 13,
             COL_UI_DIM);
}

// ---------------------------------------------------------------------------
// DrawHUD
// ---------------------------------------------------------------------------
void DrawHUD(InventorySlot *inventory, int screenWidth)
{
    int count = 0;
    for (int i = 0; i < MAX_INVENTORY; i++) {
        if (inventory[i].occupied) count++;
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "PACK: %d/%d", count, MAX_INVENTORY);

    int fontSize = 20;
    int textW    = MeasureText(buf, fontSize);
    int padX     = 12;
    int padY     = 8;
    int rectX    = screenWidth - textW - padX * 2 - 10;
    int rectY    = 10;
    int rectW    = textW + padX * 2;
    int rectH    = fontSize + padY * 2;

    DrawRectangle(rectX, rectY, rectW, rectH, COL_UI_BG);
    DrawRectangleLines(rectX, rectY, rectW, rectH, COL_UI_BORDER);
    DrawText(buf, rectX + padX, rectY + padY, fontSize, COL_UI_HEADER);
}

// ---------------------------------------------------------------------------
// DrawAtmosphere
// ---------------------------------------------------------------------------
void DrawAtmosphere(Camera2D camera, int screenWidth, int screenHeight)
{
    (void)camera;

    // Soft screen-edge vignette (dark corners, alpha ~25)
    int fadeSize = 100;
    Color vigColor = { 26, 26, 26, 25 };
    Color vigClear = { 26, 26, 26, 0  };

    DrawRectangleGradientV(0, 0, screenWidth, fadeSize,              vigColor, vigClear);
    DrawRectangleGradientV(0, screenHeight - fadeSize, screenWidth, fadeSize, vigClear, vigColor);
    DrawRectangleGradientH(0, 0, fadeSize, screenHeight,             vigColor, vigClear);
    DrawRectangleGradientH(screenWidth - fadeSize, 0, fadeSize, screenHeight, vigClear, vigColor);

    // Subtle warm haze at top/bottom edges
    int hazeSize = 60;
    Color hazeColor = { 212, 196, 168, 30 };
    Color hazeClear = { 212, 196, 168, 0  };
    DrawRectangleGradientV(0, 0, screenWidth, hazeSize,              hazeColor, hazeClear);
    DrawRectangleGradientV(0, screenHeight - hazeSize, screenWidth, hazeSize,  hazeClear, hazeColor);
}

// ---------------------------------------------------------------------------
// DrawSpawnShimmers  (world space, above ground, below items)
// Starburst: 8 lines radiating outward, growing from 2px to 12px,
// warm gold color fading from alpha 200 to 0 over 1 second.
// ---------------------------------------------------------------------------
void DrawSpawnShimmers(SpawnShimmer *shimmers, int count)
{
    for (int i = 0; i < count; i++) {
        if (!shimmers[i].active) continue;
        float t = shimmers[i].timer; // 1.0 -> 0.0
        // progress 0..1 (0 = just started, 1 = fully faded)
        float progress  = 1.0f - t;
        float lineLen   = 2.0f + progress * 10.0f;   // 2px..12px
        unsigned char a = (unsigned char)(t * 200.0f);// 200..0
        Color col = { 255, 208, 112, a };

        float cx = shimmers[i].position.x;
        float cy = shimmers[i].position.y;

        int numRays = 8;
        for (int r = 0; r < numRays; r++) {
            float angle = (float)r * (2.0f * PI / (float)numRays);
            float startDist = 4.0f + progress * 6.0f; // rays start a bit away from center
            Vector2 from = {
                cx + cosf(angle) * startDist,
                cy + sinf(angle) * startDist
            };
            Vector2 to = {
                cx + cosf(angle) * (startDist + lineLen),
                cy + sinf(angle) * (startDist + lineLen)
            };
            DrawLineEx(from, to, 1.5f, col);
        }
    }
}
