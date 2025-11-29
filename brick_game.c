#include "brick_game.h"
#include "GUI.h"
#include "LCD.h"
#include "cmsis_os2.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h> // For abs()
#include "input.h"
#include "sound.h"  // <-- sound integration (PB4 beeps)

/************************************************************
 * BRICK BREAKER – MULTI-LEVEL ENGINE (WITH SOUND)
 ************************************************************/

#define PADDLE_W        40
#define PADDLE_H        6
#define PADDLE_STEP     15

#define BALL_SIZE       6
#define BASE_SPEED_X    3
#define BASE_SPEED_Y    -3

#define BRICK_ROWS      5   // Increased rows for complex patterns
#define BRICK_COLS      8
#define BRICK_GAP       2
#define BRICK_H         10

#define MAX_LEVELS      3
#define GAME_SPEED_MS   25

typedef struct { int x, y, w, h; } rect_t;
typedef struct { int x, y, vx, vy; } ball_t;
typedef struct { rect_t rect; int active; } brick_t;

/*********** GLOBAL GAME STATE ***********/
static int screen_w, screen_h;
static rect_t paddle;
static ball_t ball;
static brick_t bricks[BRICK_ROWS][BRICK_COLS];

static int score;
static int bricks_remaining;
static int current_level;
static int game_active;
static int game_won; // 0 = playing, 1 = lost, 2 = won game

/*********** INTERNAL PROTOTYPES ***********/
static void start_new_game(void);
static void load_level(int level);
static void draw_scene(void);
static void update_physics(void);
static void move_paddle(int dir);
static int  check_collision(rect_t r1, rect_t r2);
static void draw_overlay_message(void);

/************************************************************
 * ENTRY POINT
 ************************************************************/
void StartBrickGame(void)
{
    GUI_Clear();
    screen_w = LCD_GetXSize();
    screen_h = LCD_GetYSize();

    Sound_Init();          // initialize buzzer (PB4)
    start_new_game();

    while (1)
    {
        /* --- INPUT --- */
        char key = Keypad_Get_Key();

        if (key == '4')      move_paddle(-1);
        else if (key == '6') move_paddle(1);
        else if (key == '#') return;
        else if (key == 'B') start_new_game(); // Force Restart

        /* --- LOGIC --- */
        if (game_active) {
            update_physics();
        }

        /* --- RENDER --- */
        draw_scene();

        /* --- OVERLAYS & WAITS --- */
        if (!game_active)
        {
            draw_overlay_message();
            Sound_GameOverBeep();
            // Blocking wait for restart or exit
            while (1) {
                char k = Keypad_Get_Key();
                if (k == 'B') {
                    start_new_game();
                    break;
                }
                if (k == '#') return;
                osDelay(50);
            }
        }
        else
        {
            osDelay(GAME_SPEED_MS);
        }
    }
}

/************************************************************
 * LEVEL MANAGEMENT
 ************************************************************/
static void start_new_game(void)
{
    score = 0;
    current_level = 1;
    game_won = 0;
    load_level(current_level);
}

static void load_level(int level)
{
    game_active = 1;

    /* 1. Reset Paddle */
    paddle.w = PADDLE_W;
    paddle.h = PADDLE_H;
    paddle.x = (screen_w / 2) - (PADDLE_W / 2);
    paddle.y = screen_h - 20;

    /* 2. Reset Ball (Increase speed slightly per level) */
    ball.x = screen_w / 2;
    ball.y = paddle.y - 12;

    // Level 1: Speed 3, Level 2: Speed 4, etc.
    int speed_boost = (level - 1);
    ball.vx = (BASE_SPEED_X + (speed_boost % 2)) * (rand()%2 ? 1 : -1);
    ball.vy = BASE_SPEED_Y - speed_boost;

    /* 3. Generate Bricks based on Layout */
    int brick_w = (screen_w - (BRICK_GAP * (BRICK_COLS + 1))) / BRICK_COLS;
    bricks_remaining = 0;

    for (int r = 0; r < BRICK_ROWS; r++)
    {
        for (int c = 0; c < BRICK_COLS; c++)
        {
            bricks[r][c].rect.w = brick_w;
            bricks[r][c].rect.h = BRICK_H;
            bricks[r][c].rect.x = BRICK_GAP + (c * (brick_w + BRICK_GAP));
            bricks[r][c].rect.y = BRICK_GAP + (r * (BRICK_H + BRICK_GAP)) + 25;

            int is_active = 0;

            // --- LEVEL LAYOUT LOGIC ---
            switch (level)
            {
                case 1: // STANDARD: Top 3 rows full
                    if (r < 3) is_active = 1;
                    break;

                case 2: // PILLARS: Vertical stripes
                    if (c % 2 == 0) is_active = 1;
                    break;

                case 3: // PYRAMID: Inverted triangle
                    // Row 0: All, Row 1: Middle 6, Row 2: Middle 4...
                    if (c >= r && c < (BRICK_COLS - r)) is_active = 1;
                    break;

                default: // Fallback
                    if (r < 2) is_active = 1;
                    break;
            }

            bricks[r][c].active = is_active;
            if (is_active) bricks_remaining++;
        }
    }

    // Slight pause before level starts
    GUI_Clear();
    draw_scene();
    GUI_SetFont(GUI_FONT_20_ASCII);
    GUI_SetColor(GUI_CYAN);
    GUI_DispStringHCenterAt("LEVEL UP", screen_w/2, screen_h/2);
    osDelay(800);
}

/************************************************************
 * PHYSICS
 ************************************************************/
static void update_physics(void)
{
    /* Move Ball */
    ball.x += ball.vx;
    ball.y += ball.vy;

    /* Wall Collisions */
    if (ball.x <= 0) { ball.x = 0; ball.vx = -ball.vx; }
    else if (ball.x >= screen_w - BALL_SIZE) { ball.x = screen_w - BALL_SIZE; ball.vx = -ball.vx; }

    if (ball.y <= 0) { ball.y = 0; ball.vy = -ball.vy; }
    else if (ball.y >= screen_h) {
        game_active = 0;
        game_won = 1; // 1 = Loss

        Sound_GameOverBeep();   // <<< play game-over beep

        return;
    }

    /* Paddle Collision */
    rect_t ball_rect = { ball.x, ball.y, BALL_SIZE, BALL_SIZE };
    if (check_collision(ball_rect, paddle)) {
        ball.vy = -abs(ball.vy);
        ball.y = paddle.y - BALL_SIZE - 1;
    }

    /* Brick Collision */
    for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 0; c < BRICK_COLS; c++) {
            if (bricks[r][c].active) {
                if (check_collision(ball_rect, bricks[r][c].rect)) {
                    bricks[r][c].active = 0;
                    ball.vy = -ball.vy;
                    score += 10;
                    bricks_remaining--;

                    /* LEVEL COMPLETE CHECK */
                    if (bricks_remaining == 0) {
                        if (current_level < MAX_LEVELS) {
                            current_level++;
                            load_level(current_level);
                        } else {
                            game_active = 0;
                            game_won = 2; // 2 = Victory

                            Sound_GameOverBeep();   // <<< play win beep (same long beep)
                        }
                    }
                    return;
                }
            }
        }
    }
}

/************************************************************
 * UTILS & DRAWING
 ************************************************************/
static void move_paddle(int dir)
{
    if (dir < 0) paddle.x -= PADDLE_STEP;
    else         paddle.x += PADDLE_STEP;

    if (paddle.x < 0) paddle.x = 0;
    if (paddle.x + paddle.w > screen_w) paddle.x = screen_w - paddle.w;
}

static int check_collision(rect_t r1, rect_t r2)
{
    return (r1.x < r2.x + r2.w && r1.x + r1.w > r2.x &&
            r1.y < r2.y + r2.h && r1.y + r1.h > r2.y);
}

static void draw_scene(void)
{
    GUI_SetBkColor(GUI_BLACK);
    GUI_Clear();

    /* Paddle & Ball */
    GUI_SetColor(GUI_BLUE);
    GUI_FillRect(paddle.x, paddle.y, paddle.x + paddle.w, paddle.y + paddle.h);
    GUI_SetColor(GUI_RED);
    GUI_FillRect(ball.x, ball.y, ball.x + BALL_SIZE, ball.y + BALL_SIZE);

    /* Bricks */
    for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 0; c < BRICK_COLS; c++) {
            if (bricks[r][c].active) {
                // Color based on row
                GUI_SetColor((r % 2 == 0) ? GUI_GREEN : GUI_YELLOW);
                rect_t b = bricks[r][c].rect;
                GUI_FillRect(b.x, b.y, b.x + b.w, b.y + b.h);
            }
        }
    }

    /* HUD */
    GUI_SetColor(GUI_WHITE);
    GUI_SetFont(GUI_FONT_13_ASCII);
    char buf[40];
    sprintf(buf, "LVL:%d  PTS:%d", current_level, score);
    GUI_DispStringAt(buf, 2, 2);
}

static void draw_overlay_message(void)
{
    GUI_SetFont(GUI_FONT_20_ASCII);
    if (game_won == 2) {
        GUI_SetColor(GUI_GREEN);
        GUI_DispStringHCenterAt("ALL LEVELS CLEARED!", screen_w/2, screen_h/2 - 10);
    } else {
        GUI_SetColor(GUI_RED);
        GUI_DispStringHCenterAt("GAME OVER", screen_w/2, screen_h/2 - 10);
    }

    GUI_SetFont(GUI_FONT_13_ASCII);
    GUI_SetColor(GUI_WHITE);
    GUI_DispStringHCenterAt("Press 'B' to Restart", screen_w/2, screen_h/2 + 15);
}
