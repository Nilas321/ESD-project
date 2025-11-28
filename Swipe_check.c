#include "swipe_check.h"
#include "input.h"
#include "GUI.h"
#include "stm32f4xx.h" 
#include <stdio.h>
#include <stdlib.h>

/* =========================================================================
   CONFIGURATION 
   ========================================================================= */
#define SWIPE_THRESHOLD      45 
#define SWIPE_DELAY_CYCLES   1500000 
#define SENSOR_REFRESH_DELAY 200000

// STABLE RELEASE: How many consecutive "No Touch" reads required?
#define RELEASE_CONFIRM_COUNT 10 

// COOLDOWN: Extra dead time AFTER release (approx 150-200ms)
#define COOLDOWN_CYCLES      3000000

static int16_t debug_x = 0;
static int16_t debug_y = 0;

/* =========================================================================
   CORE LOGIC: SNAPSHOT + RELEASE CHECK + COOLDOWN
   ========================================================================= */
Swipe_Dir_t Touch_Update_Swipe(void) {
    int16_t start_x, start_y;
    int16_t end_x, end_y;
    int16_t dum_x, dum_y;
    
    // 1. Detect Initial Touch
    if (Touch_GetCoord(&start_x, &start_y)) {
        
        debug_x = start_x;
        debug_y = start_y;

        // 2. LONG WAIT (Movement Window)
        for(volatile int i=0; i<SWIPE_DELAY_CYCLES; i++); 

        // 3. FLUSH OLD DATA
        Touch_GetCoord(&dum_x, &dum_y);

        // 4. SHORT WAIT (Refresh Sensor)
        for(volatile int i=0; i<SENSOR_REFRESH_DELAY; i++); 

        // 5. CAPTURE END POINT
        if (Touch_GetCoord(&end_x, &end_y)) {
            
            debug_x = end_x;
            debug_y = end_y;

            int16_t dx = end_x - start_x;
            int16_t dy = end_y - start_y;

            // 6. CALCULATE VECTOR
            if (abs(dx) > SWIPE_THRESHOLD || abs(dy) > SWIPE_THRESHOLD) {
                
                Swipe_Dir_t result = SWIPE_NONE;

                if (abs(dx) > abs(dy)) {
                    // NOTE: Check if your X axis is inverted.
                    // If Right Swipe registers as Left, swap these.
                    result = (dx < 0) ? SWIPE_RIGHT : SWIPE_LEFT; 
                } else {
                    result = (dy < 0) ? SWIPE_UP : SWIPE_DOWN;
                }

                /* ==========================================================
                   STEP A: STABLE RELEASE CHECK
                   Wait until we are 100% sure the finger is gone.
                   ========================================================== */
                int no_touch_count = 0;
                
                while(no_touch_count < RELEASE_CONFIRM_COUNT) {
                    if(Touch_GetCoord(&dum_x, &dum_y)) {
                        // Finger detected! Reset counter.
                        no_touch_count = 0;
                        debug_x = dum_x; debug_y = dum_y;
                    } else {
                        // No finger? Count up.
                        no_touch_count++;
                    }
                    // Small delay between checks
                    for(volatile int k=0; k<50000; k++);
                }

                /* ==========================================================
                   STEP B: COOLDOWN (DEBOUNCE)
                   Now that the finger is gone, wait an EXTRA moment.
                   This prevents accidental double-triggers if you tap 
                   the screen immediately after swiping.
                   ========================================================== */
                for(volatile int c=0; c<COOLDOWN_CYCLES; c++);

                // Reset debugs
                debug_x = 0;
                debug_y = 0;

                return result;
            }
        }
    }
    
    return SWIPE_NONE;
}

/* =========================================================================
   VISUAL TEST MENU
   ========================================================================= */
void StartSwipeCheck(void) {
    GUI_Clear();
    GUI_SetBkColor(GUI_BLACK);
    GUI_SetColor(GUI_WHITE);
    
    GUI_SetFont(GUI_FONT_20_ASCII);
    GUI_DispStringHCenterAt("FULL SAFE SWIPE TEST", 120, 10);
    GUI_DrawLine(0, 35, 240, 35);
    
    GUI_SetFont(GUI_FONT_16_ASCII);
    GUI_DispStringHCenterAt("Release + Cooldown", 120, 40);
    GUI_DispStringHCenterAt("Press 'D' to Exit", 120, 300);

    char buf[32];

    while(1) {
        Swipe_Dir_t dir = Touch_Update_Swipe();
        
        GUI_SetColor(GUI_YELLOW);
        GUI_SetFont(GUI_FONT_16_ASCII);
        sprintf(buf, "X: %03d   Y: %03d   ", debug_x, debug_y); 
        GUI_DispStringAt(buf, 10, 60);
        
        if(debug_x > 0) {
            GUI_SetColor(GUI_RED);
            GUI_FillCircle(debug_x, debug_y, 2);
        } else {
            GUI_SetColor(GUI_GRAY);
            GUI_DrawCircle(100, 60, 2); 
        }

        if (Keypad_Get_Key() == 'D') break; 

        if (dir != SWIPE_NONE) {
            GUI_SetColor(GUI_GREEN);
            GUI_SetFont(GUI_FONT_32B_ASCII);
            GUI_SetBkColor(GUI_BLACK); 
            GUI_ClearRect(0, 100, 240, 250); 

            switch(dir) {
                case SWIPE_UP:    GUI_DispStringHCenterAt("UP", 120, 140); break;
                case SWIPE_DOWN:  GUI_DispStringHCenterAt("DOWN", 120, 140); break;
                case SWIPE_LEFT:  GUI_DispStringHCenterAt("LEFT", 120, 140); break;
                case SWIPE_RIGHT: GUI_DispStringHCenterAt("RIGHT", 120, 140); break;
                default: break;
            }
            
            for(volatile int i=0; i<4000000; i++); 
            GUI_ClearRect(0, 100, 240, 250);
        }
    }
}