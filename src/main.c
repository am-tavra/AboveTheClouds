#include "raylib.h"

int main(void)
{
    const int screenWidth = 1280;
    const int screenHeight = 720;

    InitWindow(screenWidth, screenHeight, "Above the Clouds");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground((Color){ 244, 218, 165, 255 });

        const char *text = "Above the Clouds - POC";
        int fontSize = 40;
        int textWidth = MeasureText(text, fontSize);
        DrawText(text,
                 (screenWidth - textWidth) / 2,
                 (screenHeight - fontSize) / 2,
                 fontSize,
                 DARKBROWN);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
