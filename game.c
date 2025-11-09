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
    int dirX;
    int dirY;
} snake;

position food;
snake playerSnake;


void InitGame() {
    playerSnake.snakeHead.X = GRID_WIDTH / 2;
    playerSnake.snakeHead.Y = GRID_HEIGHT / 2;
    playerSnake.length = 1;
    playerSnake.dirX = 0;
    playerSnake.dirY = -1;

    food.X = rand() % GRID_WIDTH;
    food.Y = rand() % GRID_HEIGHT;
}

void logic(){
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    playerSnake.dirX = 0, playerSnake.dirY = -1;
    else if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  playerSnake.dirX = 0, playerSnake.dirY = 1;
    else if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  playerSnake.dirX = -1, playerSnake.dirY = 0;
    else if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) playerSnake.dirX = 1, playerSnake.dirY = 0;
   

    // Update snake body
    for (int i = playerSnake.length - 1; i > 0; i--) {
        playerSnake.body[i] = playerSnake.body[i - 1];
    }
    if (playerSnake.length > 0) {
        playerSnake.body[0] = playerSnake.snakeHead;
    }

    playerSnake.snakeHead.X += playerSnake.dirX;
    playerSnake.snakeHead.Y += playerSnake.dirY;

    for (int i = 1; i < playerSnake.length; i++) {
    if (playerSnake.snakeHead.X == playerSnake.body[i].X &&
        playerSnake.snakeHead.Y == playerSnake.body[i].Y) {
        // Collision with self
        InitGame(); // reset
    }
}

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

}