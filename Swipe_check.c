#include "swipe_check.h"
#include "input.h"
#include "GUI.h"
#include "stm32f4xx.h" 
#include <stdio.h>
#include <stdlib.h>

// ... (Keep existing definitions, variables, and Swipe_Check function here) ...
// ... (The code for Swipe_Check function I provided previously) ...

/* =========================================================================
   VISUAL TEST MODE
   ========================================================================= */
void StartSwipeCheck(void) {
    GUI_Clear();
    GUI_SetBkColor(GUI_BLACK);
    GUI_SetColor(GUI_WHITE);
    GUI_SetFont(GUI_FONT_20_ASCII);
    
    // Draw Instructions
    GUI_DispStringHCenterAt("SWIPE TEST MODE", 120, 10);
    GUI_SetFont(GUI_FONT_16_ASCII);
    GUI_DispStringHCenterAt("Swipe screen to test", 120, 40);
    GUI_DispStringHCenterAt("Press 'D' to Exit", 120, 280);

    // Draw center box reference
    GUI_DrawRect(60, 100, 180, 220); 

    int16_t x = 0, y = 0;

    while(1) {
        // 1. Check for Swipe
        Swipe_Dir_t dir = Swipe_Check();
        
        // 2. Check for Exit Key
        char key = Keypad_Get_Key();
        if (key == 'D') break; 

        // 3. Update Display based on Swipe
        if (dir != SWIPE_NONE) {
            GUI_SetColor(GUI_GREEN);
            GUI_SetFont(GUI_FONT_32B_ASCII);
            
            // Clear previous text area
            GUI_SetBkColor(GUI_BLACK); 
            GUI_ClearRect(0, 120, 240, 200);

            switch(dir) {
                case SWIPE_UP:
                    GUI_DispStringHCenterAt("UP", 120, 150);
                    break;
                case SWIPE_DOWN:
                    GUI_DispStringHCenterAt("DOWN", 120, 150);
                    break;
                case SWIPE_LEFT:
                    GUI_DispStringHCenterAt("LEFT", 120, 150);
                    break;
                case SWIPE_RIGHT:
                    GUI_DispStringHCenterAt("RIGHT", 120, 150);
                    break;
                default: break;
            }
            
            // Short delay to keep text visible before next check
            for(volatile int i=0; i<500000; i++); 
        }

        // 4. Debug: Show Raw Touch Coordinates (Optional)
        if (Touch_GetCoord(&x, &y)) {
            char buf[20];
            GUI_SetColor(GUI_YELLOW);
            GUI_SetFont(GUI_FONT_13_1);
            sprintf(buf, "%03d, %03d", x, y);
            GUI_DispStringAt(buf, 5, 300);
        }
        
        // Very short delay for loop stability
        for(volatile int i=0; i<10000; i++);
    }
}