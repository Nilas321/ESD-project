#include "2048_game.h"
#include "GUI.h"
#include "LCD.h"
#include "cmsis_os2.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "input.h"

/************************************************************
 * 2048 GAME ENGINE
 ************************************************************/

#define GRID_SIZE       4
#define CELL_PADDING    4
#define GAME_SPEED_MS   50

/* UI Dimensions */
static int BOX_SIZE; 
static int OFFSET_X;
static int OFFSET_Y;

typedef enum { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT } dir_t;

/*********** GLOBAL GAME STATE ***********/
static int board[GRID_SIZE][GRID_SIZE];
static int score;
static int game_over;
static int victory; 

/*********** INTERNAL PROTOTYPES ***********/
static void init_game(void);
static void draw_scene(void);
static void spawn_tile(void);
static int  move_board(dir_t dir);
static int  can_move(void);
static GUI_COLOR get_tile_color(int val);
static void draw_game_over(void);

// Helper for logic
static void rotate_board(void);
static int  slide_and_merge_left(void);

/************************************************************
 * PUBLIC ENTRY POINT
 ************************************************************/
void Start2048Game(void)
{
    GUI_Clear();
    
    // 1. Define Debounce State Variable
    static char last_key = 0;

    // Dynamic Layout Calculation
    int scr_w = LCD_GetXSize();
    int scr_h = LCD_GetYSize();
    
    int min_dim = (scr_w < scr_h) ? scr_w : scr_h;
    
    BOX_SIZE = (min_dim - 20) / GRID_SIZE; 
    if (BOX_SIZE < 10) BOX_SIZE = 10; 

    OFFSET_X = (scr_w - (BOX_SIZE * GRID_SIZE)) / 2;
    OFFSET_Y = (scr_h - (BOX_SIZE * GRID_SIZE)) / 2 + 10; 

    init_game();

    while (1)
    {
        /* ------------------------------
         * INPUT CONTROL & DEBOUNCE
         * ------------------------------ */
        char current_key = Keypad_Get_Key();
        int moved = 0;

        // CHECK: Key is pressed AND is different from previous frame
        if (current_key != 0 && current_key != last_key) 
        {
            if (!game_over) {
                if (current_key == '2')      moved = move_board(DIR_UP);
                else if (current_key == '8') moved = move_board(DIR_DOWN);
                else if (current_key == '4') moved = move_board(DIR_LEFT);
                else if (current_key == '6') moved = move_board(DIR_RIGHT);
            }

            /* System Keys */
            if (current_key == '#') return;
            if (current_key == 'D') {
                init_game();
                osDelay(200);
            }

            /* ------------------------------
             * MISSING LOGIC RESTORED HERE
             * ------------------------------ */
            // We only need to check this if a key was actually pressed
            if (moved) {
                spawn_tile(); // Add new '2' or '4'
                
                if (!can_move()) {
                    game_over = 1;
                }
            }
        }
        
        // Update history for next frame
        last_key = current_key;

        /* ------------------------------
         * RENDER
         * ------------------------------ */
        draw_scene();

        if (game_over) {
            draw_game_over();
            
            // Wait loop for Game Over screen
            while (1) {
                char k = Keypad_Get_Key();
                
                if (k == 'D') {
                    init_game();
                    osDelay(200);
                    break;
                }
                
                if (k == '#') return;
                
                osDelay(50);
            }
        }
        else {
            osDelay(GAME_SPEED_MS); 
        }
    }
}

/************************************************************
 * INITIALIZATION
 ************************************************************/
static void init_game(void)
{
    score = 0;
    game_over = 0;
    victory = 0;

    // Clear Board
    for (int r = 0; r < GRID_SIZE; r++) {
        for (int c = 0; c < GRID_SIZE; c++) {
            board[r][c] = 0;
        }
    }

    // Spawn 2 initial tiles
    spawn_tile();
    spawn_tile();
}

static void spawn_tile(void)
{
    struct { int r, c; } empty[GRID_SIZE * GRID_SIZE];
    int count = 0;

    for (int r = 0; r < GRID_SIZE; r++) {
        for (int c = 0; c < GRID_SIZE; c++) {
            if (board[r][c] == 0) {
                empty[count].r = r;
                empty[count].c = c;
                count++;
            }
        }
    }

    if (count > 0) {
        int idx = rand() % count;
        // 10% chance of a 4, 90% chance of a 2
        board[empty[idx].r][empty[idx].c] = (rand() % 10 == 0) ? 4 : 2;
    }
}

/************************************************************
 * CORE LOGIC (SLIDE & MERGE)
 ************************************************************/

static void rotate_board(void)
{
    int temp[GRID_SIZE][GRID_SIZE];
    // Rotate 90 degrees clockwise
    for (int r = 0; r < GRID_SIZE; r++) {
        for (int c = 0; c < GRID_SIZE; c++) {
            temp[c][GRID_SIZE - 1 - r] = board[r][c];
        }
    }
    // Copy back
    for (int r = 0; r < GRID_SIZE; r++) {
        for (int c = 0; c < GRID_SIZE; c++) {
            board[r][c] = temp[r][c];
        }
    }
}

static int slide_and_merge_left(void)
{
    int success = 0;

    for (int r = 0; r < GRID_SIZE; r++)
    {
        // 1. Shift non-zeros to the left (compress)
        int temp_row[GRID_SIZE] = {0};
        int pos = 0;
        for (int c = 0; c < GRID_SIZE; c++) {
            if (board[r][c] != 0) {
                temp_row[pos++] = board[r][c];
            }
        }

        // 2. Merge adjacent equals
        for (int i = 0; i < pos - 1; i++) {
            if (temp_row[i] == temp_row[i+1] && temp_row[i] != 0) {
                temp_row[i] *= 2;
                score += temp_row[i];
                if (temp_row[i] == 2048) victory = 1;
                temp_row[i+1] = 0;
                break;// Skip next since it was merged
            }
						
        }

       // 3. Shift again (compress after merge)
        int final_row[GRID_SIZE] = {0};
        pos = 0;
        for (int i = 0; i < GRID_SIZE; i++) {
            if (temp_row[i] != 0) {
                final_row[pos++] = temp_row[i];
            }
        }

        // 4. Update board and check for changes
        for (int c = 0; c < GRID_SIZE; c++) {
            if (board[r][c] != final_row[c]) {
                board[r][c] = final_row[c];
                success = 1;
            }
        }
    }
    return success;
}

static int move_board(dir_t dir)
{
    int moved = 0;
    
    switch (dir) {
        case DIR_LEFT:
            moved = slide_and_merge_left();
            break;
        case DIR_RIGHT:
            rotate_board(); rotate_board();
            moved = slide_and_merge_left();
            rotate_board(); rotate_board();
            break;
        case DIR_UP:
            rotate_board(); rotate_board(); rotate_board();
            moved = slide_and_merge_left();
            rotate_board();
            break;
        case DIR_DOWN:
            rotate_board();
            moved = slide_and_merge_left();
            rotate_board(); rotate_board(); rotate_board();
            break;
    }
    return moved;
}

static int can_move(void)
{
    for (int r = 0; r < GRID_SIZE; r++) {
        for (int c = 0; c < GRID_SIZE; c++) {
            if (board[r][c] == 0) return 1; // Empty spot
            if (c < GRID_SIZE - 1 && board[r][c] == board[r][c+1]) return 1; // Right neighbor
            if (r < GRID_SIZE - 1 && board[r][c] == board[r+1][c]) return 1; // Bottom neighbor
        }
    }
    return 0;
}

/************************************************************
 * RENDERING
 ************************************************************/

static GUI_COLOR get_tile_color(int val)
{
    switch(val) {
        case 0:    return GUI_GRAY;
        case 2:    return 0x00EEEEEE; 
        case 4:    return 0x00CCEEFF; 
        case 8:    return 0x0077BBFF; 
        case 16:   return 0x005588FF; 
        case 32:   return 0x005555FF; 
        case 64:   return 0x000000FF; 
        case 128:  return 0x0077DDDD; 
        case 256:  return 0x0066CCCC; 
        case 512:  return 0x0055BBBB; 
        case 1024: return 0x0000AAAA; 
        case 2048: return 0x0000FFFF; 
        default:   return GUI_BLACK;
    }
}

static void draw_scene(void)
{
    GUI_SetBkColor(0x00444444); 
    GUI_Clear();

    /* Draw Header */
    GUI_SetColor(GUI_WHITE);
    GUI_SetFont(GUI_FONT_20_ASCII);
    char score_buf[32];
    sprintf(score_buf, "SCORE: %d", score);
    GUI_DispStringHCenterAt(score_buf, LCD_GetXSize() / 2, 5);

    /* Draw Grid */
    for (int r = 0; r < GRID_SIZE; r++)
    {
        for (int c = 0; c < GRID_SIZE; c++)
        {
            int val = board[r][c];
            int x0 = OFFSET_X + (c * BOX_SIZE) + CELL_PADDING;
            int y0 = OFFSET_Y + (r * BOX_SIZE) + CELL_PADDING;
            int x1 = x0 + BOX_SIZE - (CELL_PADDING * 2);
            int y1 = y0 + BOX_SIZE - (CELL_PADDING * 2);

            // Draw Tile Background
            GUI_SetColor(get_tile_color(val));
            GUI_FillRect(x0, y0, x1, y1);

            // Draw Number
           // Draw Number
            if (val > 0) {
                // CHANGED: Force Black color for all numbers
                GUI_SetColor(GUI_WHITE);         
                
                // Adjust font size based on number length
                if (val < 100) GUI_SetFont(GUI_FONT_32B_ASCII);
                else if (val < 1000) GUI_SetFont(GUI_FONT_24B_ASCII);
                else GUI_SetFont(GUI_FONT_20_ASCII);

                char num_buf[8];
                sprintf(num_buf, "%d", val);
                
                // Calculate centering
                int tx_size = GUI_GetStringDistX(num_buf);
                int ty_size = GUI_GetFontSizeY();
                
                int tx = x0 + ((x1 - x0) - tx_size) / 2;
                int ty = y0 + ((y1 - y0) - ty_size) / 2;
                
                GUI_DispStringAt(num_buf, tx, ty);
            }
        }
    }
}

static void draw_game_over(void)
{
    int w = 180;
    int h = 100;
    int x = (LCD_GetXSize() - w) / 2;
    int y = (LCD_GetYSize() - h) / 2;

    GUI_SetColor(GUI_WHITE);
    GUI_FillRect(x, y, x + w, y + h);
    GUI_SetColor(GUI_BLACK);
    GUI_DrawRect(x, y, x + w, y + h);

    GUI_SetFont(GUI_FONT_24B_ASCII);
    
    if (victory) {
        GUI_SetColor(GUI_GREEN);
        GUI_DispStringHCenterAt("2048 REACHED!", LCD_GetXSize()/2, y + 20);
    } else {
        GUI_SetColor(GUI_RED);
        GUI_DispStringHCenterAt("GAME OVER", LCD_GetXSize()/2, y + 20);
    }

    GUI_SetColor(GUI_BLACK);
    GUI_SetFont(GUI_FONT_16_ASCII);
    
    /* UPDATED INSTRUCTION TEXT */
    GUI_DispStringHCenterAt("Press 'D' to Restart", LCD_GetXSize()/2, y + 60);
}