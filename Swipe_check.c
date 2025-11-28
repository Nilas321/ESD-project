#include "swipe_check.h"
#include "input.h"
#include "GUI.h"
#include "stm32f4xx.h" 
#include <stdio.h>
#include <stdlib.h>

/* =========================================================================
   CONFIGURATION & STATE
   ========================================================================= */
#define SWIPE_THRESHOLD_X  50 
#define SWIPE_THRESHOLD_Y  50

static int16_t start_x = 0;
static int16_t start_y = 0;
static int16_t last_x  = 0;
static int16_t last_y  = 0;
static uint8_t is_touching = 0;

/* =========================================================================
   CORE LOGIC: DETECT SWIPES
   ========================================================================= */
Swipe_Dir_t Swipe_Check(void) {
    int16_t curr_x, curr_y;
    Swipe_Dir_t result = SWIPE_NONE;

    // Get current touch status
    int touch_detected = Touch_GetCoord(&curr_x, &curr_y);

    if (touch_detected) {
        if (!is_touching) {
            // [RISING EDGE] First touch
            start_x = curr_x;
            start_y = curr_y;
            last_x  = curr_x;
            last_y  = curr_y;
            is_touching = 1;
        } else {
            // [HOLDING] Dragging
            last_x = curr_x;
            last_y = curr_y;
        }
    } else {
        if (is_touching) {
            // [FALLING EDGE] Released
            is_touching = 0;

            int16_t dx = last_x - start_x;
            int16_t dy = last_y - start_y;
            int16_t adx = abs(dx);
            int16_t ady = abs(dy);

            if (adx > ady) {
                // Horizontal
                if (adx > SWIPE_THRESHOLD_X) {
                    result = (dx > 0) ? SWIPE_RIGHT : SWIPE_LEFT;
                }
            } else {
                // Vertical (Assuming 0 is top, 320 is bottom based on standard GLCD)
                // If your Y is inverted (0 is bottom), swap these return values.
                if (ady > SWIPE_THRESHOLD_Y) {
                    result = (dy > 0) ? SWIPE_UP : SWIPE_DOWN;
                }
            }
        }
    }
    return result;
}

/* =========================================================================
   VISUAL TEST MENU
   ========================================================================= */
void StartSwipeCheck(void) {
    GUI_Clear();
    GUI_SetBkColor(GUI_BLACK);
    GUI_SetColor(GUI_WHITE);
    GUI_SetFont(GUI_FONT_20_ASCII);
    
    GUI_DispStringHCenterAt("SWIPE TEST MODE", 120, 10);
    GUI_SetFont(GUI_FONT_16_ASCII);
    GUI_DispStringHCenterAt("Swipe screen to test", 120, 40);
    GUI_DispStringHCenterAt("Press 'D' to Exit", 120, 280);

    GUI_DrawRect(60, 100, 180, 220); 

    int16_t x = 0, y = 0;

    while(1) {
        // 1. Check for Swipe
        Swipe_Dir_t dir = Swipe_Check();
        
        // 2. Check for Exit Key
        char key = Keypad_Get_Key();
        if (key == 'D') break; 

        // 3. Update Display
        if (dir != SWIPE_NONE) {
            GUI_SetColor(GUI_GREEN);
            GUI_SetFont(GUI_FONT_32B_ASCII);
            GUI_SetBkColor(GUI_BLACK); 
            GUI_ClearRect(0, 120, 240, 200); // Clear old text

            switch(dir) {
                case SWIPE_UP:    GUI_DispStringHCenterAt("UP", 120, 150); break;
                case SWIPE_DOWN:  GUI_DispStringHCenterAt("DOWN", 120, 150); break;
                case SWIPE_LEFT:  GUI_DispStringHCenterAt("LEFT", 120, 150); break;
                case SWIPE_RIGHT: GUI_DispStringHCenterAt("RIGHT", 120, 150); break;
                default: break;
            }
            // Delay to keep text visible
            for(volatile int i=0; i<500000; i++); 
        }

        // 4. Debug Coords
        if (Touch_GetCoord(&x, &y)) {
            char buf[20];
            GUI_SetColor(GUI_YELLOW);
            GUI_SetFont(GUI_FONT_13_1);
            sprintf(buf, "%03d, %03d", x, y);
            GUI_DispStringAt(buf, 5, 300);
        }
        
        for(volatile int i=0; i<10000; i++);
    }
}