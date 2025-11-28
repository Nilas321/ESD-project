#include "GUI.h"
#include "LCD.h"
#include "cmsis_os2.h"
#include <string.h>
#include <stdint.h>

/* Button readers from input.c */
extern int btn_up_pressed(void);
extern int btn_down_pressed(void);
extern int btn_left_pressed(void);
extern int btn_right_pressed(void);
extern int btn_start_pressed(void);

/* ------------------- GRID CONFIG ------------------- */

#define CELL_SIZE        12
#define MAX_SNAKE_LEN    128
#define INITIAL_SPEED_MS 160

typedef enum {
    DIR_UP = 0,
    DIR_RIGHT,
    DIR_DOWN,
    DIR_LEFT
} dir_t;

typedef struct {
    int x;
    int y;
} cell_t;

/* Global game state */
static cell_t snake[MAX_SNAKE_LEN];
static int snake_len = 0;
static cell_t fruit;
static dir_t cur_dir;
static int grid_w, grid_h, pixel_w, pixel_h;

/* ------------------- RNG ------------------- */
static uint32_t rng_state = 0x12345678;
static uint32_t rng_next(void)
{
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return rng_state = x;
}

/* ------------------- FRUIT ------------------- */
static void place_fruit(void)
{
    while (1)
    {
        int rx = rng_next() % grid_w;
        int ry = rng_next() % grid_h;

        int collision = 0;
        for (int i = 0; i < snake_len; i++)
        {
            if (snake[i].x == rx && snake[i].y == ry)
            {
                collision = 1;
                break;
            }
        }

        if (!collision)
        {
            fruit.x = rx;
            fruit.y = ry;
            return;
        }
    }
}

/* ------------------- INIT GAME ------------------- */
static void init_game(void)
{
    pixel_w = LCD_GetXSize();
    pixel_h = LCD_GetYSize();

    grid_w = pixel_w / CELL_SIZE;
    grid_h = pixel_h / CELL_SIZE;

    if (grid_w < 10) grid_w = 10;
    if (grid_h < 10) grid_h = 10;

    int cx = grid_w / 2;
    int cy = grid_h / 2;

    snake_len = 4;
    for (int i = 0; i < snake_len; i++)
    {
        snake[i].x = cx - i;
        snake[i].y = cy;
    }

    cur_dir = DIR_RIGHT;
    place_fruit();
}

/* ------------------- DRAW HELPERS ------------------- */

static void draw_cell(int gx, int gy, int filled)
{
    int x = gx * CELL_SIZE;
    int y = gy * CELL_SIZE;

    if (filled)
        GUI_FillRect(x, y, x + CELL_SIZE - 1, y + CELL_SIZE - 1);
    else
        GUI_DrawRect(x, y, x + CELL_SIZE - 1, y + CELL_SIZE - 1);
}

static void draw_scene(void)
{
    GUI_SetBkColor(GUI_BLACK);
    GUI_Clear();

    GUI_SetColor(GUI_RED);
    draw_cell(fruit.x, fruit.y, 1);

    GUI_SetColor(GUI_GREEN);
    for (int i = 0; i < snake_len; i++)
        draw_cell(snake[i].x, snake[i].y, 1);

    GUI_SetColor(GUI_WHITE);
    GUI_SetFont(GUI_FONT_13_ASCII);
    char buf[32];
    sprintf(buf, "LEN: %d", snake_len);
    GUI_DispStringAt(buf, 4, 4);
}

/* ------------------- COLLISION ------------------- */
static int is_collision(cell_t h)
{
    if (h.x < 0 || h.x >= grid_w || h.y < 0 || h.y >= grid_h)
        return 1;

    for (int i = 0; i < snake_len; i++)
        if (snake[i].x == h.x && snake[i].y == h.y)
            return 1;

    return 0;
}

/* ------------------- MOVEMENT ------------------- */

static dir_t read_input_dir(dir_t old)
{
    if (btn_up_pressed() && old != DIR_DOWN)
        return DIR_UP;

    if (btn_down_pressed() && old != DIR_UP)
        return DIR_DOWN;

    if (btn_left_pressed() && old != DIR_RIGHT)
        return DIR_LEFT;

    if (btn_right_pressed() && old != DIR_LEFT)
        return DIR_RIGHT;

    return old;
}

static int move_snake(void)
{
    cell_t head = snake[0];

    switch (cur_dir)
    {
        case DIR_UP:    head.y--; break;
        case DIR_DOWN:  head.y++; break;
        case DIR_LEFT:  head.x--; break;
        case DIR_RIGHT: head.x++; break;
    }

    if (is_collision(head)) return -1;

    for (int i = snake_len - 1; i > 0; i--)
        snake[i] = snake[i - 1];

    snake[0] = head;

    if (head.x == fruit.x && head.y == fruit.y)
    {
        if (snake_len < MAX_SNAKE_LEN)
            snake[snake_len++] = snake[snake_len - 1];
        place_fruit();
        return 1;
    }

    return 0;
}

/* ------------------- GAME OVER ------------------- */
static void game_over_screen(void)
{
    GUI_SetFont(GUI_FONT_20_ASCII);
    GUI_SetColor(GUI_WHITE);
    GUI_DispStringHCenterAt("GAME OVER", pixel_w / 2, pixel_h / 2 - 20);

    GUI_SetFont(GUI_FONT_13_ASCII);
    GUI_DispStringHCenterAt("Press START (PG15)", pixel_w / 2, pixel_h / 2 + 8);
}

/* ------------------- MAIN THREAD ------------------- */
__NO_RETURN void app_main(void *argument)
{
    (void)argument;

    GUI_Init();

    GUI_SetBkColor(GUI_RED); GUI_Clear(); GUI_Delay(150);
    GUI_SetBkColor(GUI_GREEN); GUI_Clear(); GUI_Delay(150);
    GUI_SetBkColor(GUI_BLUE); GUI_Clear(); GUI_Delay(150);

    init_game();
    draw_scene();

    GUI_SetColor(GUI_WHITE);
    GUI_SetFont(GUI_FONT_16_ASCII);
    GUI_DispStringHCenterAt("Press START (PG15)", pixel_w / 2, pixel_h / 2);

    while (!btn_start_pressed())
        osDelay(50);

    GUI_Clear();

    uint32_t speed = INITIAL_SPEED_MS;

    while (1)
    {
        cur_dir = read_input_dir(cur_dir);

        int result = move_snake();

        if (result < 0)
        {
            draw_scene();
            game_over_screen();

            while (!btn_start_pressed())
                osDelay(100);

            init_game();
            speed = INITIAL_SPEED_MS;
            continue;
        }

        if (result > 0 && speed > 60)
            speed -= 5;

        draw_scene();
        osDelay(speed);
    }
}
