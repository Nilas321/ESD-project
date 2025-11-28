#include "flappy_game.h"
#include "GUI.h"
#include "LCD.h"
#include "cmsis_os2.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h> 
#include "input.h" // <--- Now includes Touch_GetCoord
#include "sound.h"
/************************************************************
 * FLAPPY BIRD – STANDALONE ENGINE
 ************************************************************/

/* --- PHYSICS CONSTANTS --- */
#define GRAVITY         1      
#define JUMP_FORCE      -6     
#define PIPE_SPEED      3      
#define MAX_FALL_SPEED  8      

/* --- DIMENSIONS --- */
#define BIRD_SIZE       10
#define BIRD_X_POS      40     

#define PIPE_WIDTH      25
#define PIPE_GAP_H      45     
#define PIPE_SPACING    120     
#define NUM_PIPES       3      

#define GROUND_H        10     
#define GAME_SPEED_MS   40     

typedef struct {
    int y;
    int vel_y;
} bird_t;

typedef struct {
    int x;
    int gap_y; 
    int active;
} pipe_t;

/*********** GLOBAL GAME STATE ***********/
static int screen_w, screen_h;
static bird_t bird;
static pipe_t pipes[NUM_PIPES];
static int score;
static int game_active;
static int high_score = 0;

/*********** INTERNAL PROTOTYPES ***********/
static void init_game(void);
static void draw_scene(void);
static void update_physics(void);
static void spawn_pipe(int index, int start_x);
static int  check_collision(void);
static void game_over_screen(void);

/************************************************************
 * PUBLIC FUNCTION – ENTRY POINT
 ************************************************************/
void StartFlappyGame(void)
{
    GUI_Clear();
    screen_w = LCD_GetXSize();
    screen_h = LCD_GetYSize();

    // --- TOUCH VARIABLES ---
    int16_t tX, tY;
    int is_touched = 0;
    int prev_touched = 0; // Needed to detect single taps

    init_game();

    while (1)
    {
        /* ------------------------------
         * INPUT CONTROL
         * ------------------------------ */
        char key = Keypad_Get_Key();
        
        // 1. Poll Touch Sensor
        is_touched = Touch_GetCoord(&tX, &tY);

        /* Jump Controls (Key '5' OR Touch Tap) */
        if (game_active) {
            // Check for Key '5' OR (Touch  is active AND it wasn't active last frame)
            if (key == '5' || (is_touched)) {
                bird.vel_y = JUMP_FORCE;
            }
        }
        
        // Update history for next frame (Edge Detection)
        prev_touched = is_touched;

        /* Exit */
        if (key == '#') return;

        /* Force Restart (In-game) */
        if (key == 'C') {
            init_game();
            osDelay(200);
        }

        /* ------------------------------
         * GAME LOGIC
         * ------------------------------ */
        if (game_active) {
            update_physics();
        }

        /* ------------------------------
         * RENDER & DELAY
         * ------------------------------ */
        draw_scene();

        if (!game_active)
        {
            game_over_screen();
            Sound_GameOverBeep();  
            /* Wait for 'C' or Touch to restart */
            // 1. Small delay so we don't accidentally restart instantly
            osDelay(500);
            
            // 2. Clear any lingering touch input
            while(Touch_GetCoord(&tX, &tY));

            // 3. Wait for new input
            while (1) {
                char k = Keypad_Get_Key();
                
                // Restart on Touch
                if (Touch_GetCoord(&tX, &tY)) {
                     init_game();
                     osDelay(200);
                     break;
                }

                if (k == 'C') {
                    init_game();
                    osDelay(200);
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
 * INITIALIZE GAME
 ************************************************************/
static void init_game(void)
{
    score = 0;
    game_active = 1;

    /* Reset Bird */
    bird.y = screen_h / 2;
    bird.vel_y = 0;

    /* Initialize Pipes spread out horizontally */
    for (int i = 0; i < NUM_PIPES; i++)
    {
        spawn_pipe(i, screen_w + (i * PIPE_SPACING));
    }
}

static void spawn_pipe(int index, int start_x)
{
    pipes[index].x = start_x;
    
    /* Random gap height */
    int min_gap_y = 20;
    int max_gap_y = screen_h - GROUND_H - PIPE_GAP_H - 20;
    
    pipes[index].gap_y = min_gap_y + (rand() % (max_gap_y - min_gap_y));
    pipes[index].active = 1;
}

/************************************************************
 * PHYSICS ENGINE
 ************************************************************/
static void update_physics(void)
{
    /* 1. Apply Gravity */
    bird.vel_y += GRAVITY;
    
    /* Terminal velocity clamp */
    if (bird.vel_y > MAX_FALL_SPEED) 
        bird.vel_y = MAX_FALL_SPEED;

    bird.y += bird.vel_y;

    /* 2. Move Pipes */
    for (int i = 0; i < NUM_PIPES; i++)
    {
        pipes[i].x -= PIPE_SPEED;

        /* Recycle pipe if it goes off screen */
        if (pipes[i].x + PIPE_WIDTH < 0)
        {
            /* Find the right-most pipe to place this one behind */
            int right_most_x = 0;
            for(int j=0; j<NUM_PIPES; j++) {
                if(pipes[j].x > right_most_x) right_most_x = pipes[j].x;
            }
            
            spawn_pipe(i, right_most_x + PIPE_SPACING);
            
            /* Increment Score when a pipe is passed */
            score++;
        }
    }

    /* 3. Check Collisions */
    if (check_collision())
    {
        game_active = 0;
        if(score > high_score) high_score = score;
    }
}

static int check_collision(void)
{
    /* A. Ground / Ceiling Collision */
    if (bird.y < 0) return 1; // Hit ceiling
    if (bird.y + BIRD_SIZE >= screen_h - GROUND_H) return 1; // Hit ground

    /* B. Pipe Collision */
    int bx = BIRD_X_POS;
    int by = bird.y;
    int bw = BIRD_SIZE;
    int bh = BIRD_SIZE;

    for (int i = 0; i < NUM_PIPES; i++)
    {
        int px = pipes[i].x;
        int py = pipes[i].gap_y; /* Top of the GAP */
        
        /* Check if bird is within the pipe's horizontal area */
        if (bx + bw > px && bx < px + PIPE_WIDTH)
        {
            /* Collision if ABOVE the gap (Top Pipe) */
            if (by < py) return 1;

            /* Collision if BELOW the gap (Bottom Pipe) */
            if (by + bh > py + PIPE_GAP_H) return 1;
        }
    }
    
    return 0;
}

/************************************************************
 * DRAWING
 ************************************************************/
static void draw_scene(void)
{
    /* 1. Clear Background */
    // Note: Ensure this color format matches your LCD config (e.g. RGB565 vs ARGB8888)
    // If screen is black, try GUI_LIGHTBLUE instead
    GUI_SetBkColor(0x00FFFF00); 
    GUI_Clear();

    /* 2. Draw Pipes (Green) */
    GUI_SetColor(GUI_GREEN);
    for (int i = 0; i < NUM_PIPES; i++)
    {
        /* Top Pipe segment */
        GUI_FillRect(pipes[i].x, 0, pipes[i].x + PIPE_WIDTH, pipes[i].gap_y);

        /* Bottom Pipe segment */
        GUI_FillRect(pipes[i].x, pipes[i].gap_y + PIPE_GAP_H, 
                      pipes[i].x + PIPE_WIDTH, screen_h - GROUND_H);
        
        /* Outline */
        GUI_SetColor(GUI_BLACK);
        GUI_DrawRect(pipes[i].x, 0, pipes[i].x + PIPE_WIDTH, pipes[i].gap_y);
        GUI_DrawRect(pipes[i].x, pipes[i].gap_y + PIPE_GAP_H, 
                      pipes[i].x + PIPE_WIDTH, screen_h - GROUND_H);
        GUI_SetColor(GUI_GREEN); 
    }

    /* 3. Draw Ground (Brown) */
    GUI_SetColor(GUI_BROWN);
    GUI_FillRect(0, screen_h - GROUND_H, screen_w, screen_h);

    /* 4. Draw Bird (Yellow) */
    GUI_SetColor(GUI_YELLOW);
    GUI_FillRect(BIRD_X_POS, bird.y, BIRD_X_POS + BIRD_SIZE, bird.y + BIRD_SIZE);
    
    /* Bird Eye */
    GUI_SetColor(GUI_BLACK);
    GUI_DrawPixel(BIRD_X_POS + BIRD_SIZE - 2, bird.y + 2);

    /* 5. Draw Score */
    GUI_SetColor(GUI_BLACK); 
    GUI_SetFont(GUI_FONT_20_ASCII);
    char buf[16];
    sprintf(buf, "%d", score);
    GUI_DispStringHCenterAt(buf, screen_w / 2, 10);
}

static void game_over_screen(void)
{
    /* Overlay box */
    int box_w = 120;
    int box_h = 80;
    int box_x = (screen_w - box_w) / 2;
    int box_y = (screen_h - box_h) / 2;

    GUI_SetColor(GUI_WHITE);
    GUI_FillRect(box_x, box_y, box_x + box_w, box_y + box_h);
    GUI_SetColor(GUI_BLACK);
    GUI_DrawRect(box_x, box_y, box_x + box_w, box_y + box_h);

    /* Text */
    GUI_SetBkColor(GUI_WHITE);
    GUI_SetColor(GUI_RED);
    GUI_SetFont(GUI_FONT_20_ASCII);
    GUI_DispStringHCenterAt("GAME OVER", screen_w / 2, box_y + 10);

    GUI_SetColor(GUI_BLACK);
    GUI_SetFont(GUI_FONT_13_ASCII);
    
    char buf[32];
    sprintf(buf, "Score: %d", score);
    GUI_DispStringHCenterAt(buf, screen_w / 2, box_y + 35);
    
    sprintf(buf, "High: %d", high_score);
    GUI_DispStringHCenterAt(buf, screen_w / 2, box_y + 50);

    GUI_DispStringHCenterAt("Tap to Restart", screen_w / 2, box_y + 65);
}