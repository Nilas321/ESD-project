#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "raylib.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define GRID_SIZE 20
#define GRID_WIDTH (SCREEN_WIDTH / GRID_SIZE)
#define GRID_HEIGHT (SCREEN_HEIGHT / GRID_SIZE)

typedef struct {
    int X;
    int Y;
} position;

typedef struct {
    position snakeHead;
    int length;
    position body[GRID_WIDTH * GRID_HEIGHT];
    position dir;
} snake;

position UP = {0,-1};
position DOWN = {0,1};
position LEFT = {-1,0};
position RIGHT = {1,0}; 

position food;
snake playerSnake;

// Initialize game state
void InitGame() {
    playerSnake.snakeHead.X = GRID_WIDTH / 2;
    playerSnake.snakeHead.Y = GRID_HEIGHT / 2;
    playerSnake.length = 1;
    playerSnake.dir = UP;

    food.X = rand() % GRID_WIDTH;
    food.Y = rand() % GRID_HEIGHT;
}

// Game logic
void logic() {
    // Controls
    if ((IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) && ((playerSnake.dir.X != DOWN.X) && (playerSnake.dir.Y != DOWN.Y)))    { playerSnake.dir = UP;}
    else if ((IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN) && (playerSnake.dir.X != UP.X) && (playerSnake.dir.Y != UP.Y)))  { playerSnake.dir = DOWN; }
    else if ((IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT) && (playerSnake.dir.X != RIGHT.X) && (playerSnake.dir.Y != RIGHT.Y)))  {  playerSnake.dir = LEFT;  }
    else if ((IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT) && (playerSnake.dir.X != LEFT.X) && (playerSnake.dir.Y != LEFT.Y))) {  playerSnake.dir = RIGHT;  }

    // Move body
    for (int i = playerSnake.length - 1; i > 0; i--) {
        playerSnake.body[i] = playerSnake.body[i - 1];
    }
    if (playerSnake.length > 0) {
        playerSnake.body[0] = playerSnake.snakeHead;
    }

    // Move head
    playerSnake.snakeHead.X += playerSnake.dir.X;
    playerSnake.snakeHead.Y += playerSnake.dir.Y;

    // Wrap around screen
    if (playerSnake.snakeHead.X < 0) playerSnake.snakeHead.X = GRID_WIDTH - 1;
    if (playerSnake.snakeHead.X >= GRID_WIDTH) playerSnake.snakeHead.X = 0;
    if (playerSnake.snakeHead.Y < 0) playerSnake.snakeHead.Y = GRID_HEIGHT - 1;
    if (playerSnake.snakeHead.Y >= GRID_HEIGHT) playerSnake.snakeHead.Y = 0;

    // Check for food collision
    if (playerSnake.snakeHead.X == food.X && playerSnake.snakeHead.Y == food.Y) {
        playerSnake.length++;
        food.X = rand() % GRID_WIDTH;
        food.Y = rand() % GRID_HEIGHT;
    }

    // Check for self collision
    for (int i = 1; i < playerSnake.length; i++) {
        if (playerSnake.snakeHead.X == playerSnake.body[i].X &&
            playerSnake.snakeHead.Y == playerSnake.body[i].Y) {
            InitGame(); // Reset game
        }
    }
}

// Draw everything using Raylib
void DrawGame() {
    BeginDrawing();
    ClearBackground(BLACK);

    // Draw grid (optional)
    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            DrawRectangleLines(x * GRID_SIZE, y * GRID_SIZE, GRID_SIZE, GRID_SIZE, DARKGRAY);
        }
    }

    // Draw food
    DrawRectangle(food.X * GRID_SIZE, food.Y * GRID_SIZE, GRID_SIZE, GRID_SIZE, RED);

    // Draw snake
    DrawRectangle(playerSnake.snakeHead.X * GRID_SIZE, playerSnake.snakeHead.Y * GRID_SIZE, GRID_SIZE, GRID_SIZE, GREEN);
    for (int i = 0; i < playerSnake.length; i++) {
        DrawRectangle(playerSnake.body[i].X * GRID_SIZE, playerSnake.body[i].Y * GRID_SIZE, GRID_SIZE, GRID_SIZE, LIME);
    }

    // Draw score
    DrawText(TextFormat("Length: %d", playerSnake.length), 10, 10, 20, RAYWHITE);

    EndDrawing();
}

// ---- MAIN ----
int main(void) {
    srand(time(NULL));

    // Check Raylib window creation
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Snake Game - Raylib C");
    if (!IsWindowReady()) {
        fprintf(stderr, "Error: Could not initialize Raylib window.\n");
        return EXIT_FAILURE;
    }

    InitGame();
    SetTargetFPS(10); // Snake speed

    // Game loop
    while (!WindowShouldClose()) {
        logic();
        DrawGame();
    }

    CloseWindow(); // Clean up Raylib
    return 0;
}
