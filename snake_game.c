#include "snake_game.h"
#include "GUI.h"
#include "input.h"
#include "sound.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <stdlib.h>

#define CELL_SIZE        12
#define MAX_SNAKE_LEN    128
#define INITIAL_SPEED_MS 160

typedef struct { int x, y; } cell_t;
typedef enum { DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT } dir_t;

static cell_t snake[MAX_SNAKE_LEN];
static cell_t fruit;
static int snake_len;
static dir_t cur_dir;
static int grid_w, grid_h;

static void place_fruit(void) {
    while (1) {
        int rx = rand() % grid_w;
        int ry = rand() % grid_h;
        int ok = 1;
        for (int i = 0; i < snake_len; i++)
            if (snake[i].x == rx && snake[i].y == ry) ok = 0;
        
        if (ok) {
            fruit.x = rx; fruit.y = ry;
            return;
        }
    }
}

static void init_game(void) {
    int pixel_w = GUI_GetXSize();
    int pixel_h = GUI_GetYSize();
    grid_w = pixel_w / CELL_SIZE;
    grid_h = pixel_h / CELL_SIZE;

    snake_len = 2;
    snake[0].x = grid_w / 2;     snake[0].y = grid_h / 2;
    snake[1].x = grid_w / 2 - 1; snake[1].y = grid_h / 2;
    cur_dir = DIR_RIGHT;
    place_fruit();
}

static void draw_scene(void) {
    GUI_SetBkColor(GUI_BLACK);
    GUI_Clear();

    /* Draw Fruit */
    GUI_SetColor(GUI_RED);
    GUI_FillRect(fruit.x * CELL_SIZE, fruit.y * CELL_SIZE,
                 fruit.x * CELL_SIZE + CELL_SIZE - 1, fruit.y * CELL_SIZE + CELL_SIZE - 1);

    /* Draw Snake */
    GUI_SetColor(GUI_GREEN);
    for (int i = 0; i < snake_len; i++) {
        GUI_FillRect(snake[i].x * CELL_SIZE, snake[i].y * CELL_SIZE,
                     snake[i].x * CELL_SIZE + CELL_SIZE - 1, snake[i].y * CELL_SIZE + CELL_SIZE - 1);
    }
}

static int move_snake(void) {
    cell_t head = snake[0];
    switch (cur_dir) {
        case DIR_UP:    head.y--; break;
        case DIR_DOWN:  head.y++; break;
        case DIR_LEFT:  head.x--; break;
        case DIR_RIGHT: head.x++; break;
    }

    // Wrapping
    if (head.x < 0) head.x = grid_w - 1;
    if (head.x >= grid_w) head.x = 0;
    if (head.y < 0) head.y = grid_h - 1;
    if (head.y >= grid_h) head.y = 0;

    // Self Collision
    for (int i = 0; i < snake_len; i++)
        if (snake[i].x == head.x && snake[i].y == head.y) return -1;

    // Move Body
    for (int i = snake_len - 1; i > 0; i--)
        snake[i] = snake[i - 1];
    snake[0] = head;

    // Eat Fruit
    if (head.x == fruit.x && head.y == fruit.y) {
        if (snake_len < MAX_SNAKE_LEN)
            snake[snake_len++] = snake[snake_len - 1];
        place_fruit();
        Sound_FruitBeep(); // Audio Feedback
        return 1;
    }
    return 0;
}

void StartSnakeGame(void) {
    init_game();
    uint32_t speed = INITIAL_SPEED_MS;

    while (1) {
        /* Keypad Input Handling Only */
        char key = Keypad_Get_Key();
        if (key == '2' && cur_dir != DIR_DOWN) cur_dir = DIR_UP;
        if (key == '8' && cur_dir != DIR_UP)   cur_dir = DIR_DOWN;
        if (key == '4' && cur_dir != DIR_RIGHT)cur_dir = DIR_LEFT;
        if (key == '6' && cur_dir != DIR_LEFT) cur_dir = DIR_RIGHT;

        /* Game Logic */
        int result = move_snake();

        if (result < 0) { // Game Over
            GUI_SetColor(GUI_WHITE);
            GUI_DispStringHCenterAt("GAME OVER", GUI_GetXSize()/2, GUI_GetYSize()/2);
            Sound_GameOverBeep(); // Audio Feedback
            osDelay(2000);
            init_game();
            speed = INITIAL_SPEED_MS;
        }
        else {
            if (result > 0 && speed > 50) speed -= 5; // Speed up
            draw_scene();
            osDelay(speed);
        }
    }
}