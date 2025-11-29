#include "main.h"
#include "GUI.h"
#include "snake_game.h"
#include "input.h" 
#include "brick_game.h"
#include "flappy_game.h"
#include "2048_game.h"
#include "Swipe_check.h"
#include <stdio.h> 

/* ==========================================
   MENU LOGIC
   ========================================== */

void DrawMainMenu(void) {
    GUI_SetBkColor(GUI_BLACK);
    GUI_Clear();
    GUI_SetColor(GUI_WHITE);
    GUI_SetFont(GUI_FONT_20F_ASCII);
    
    GUI_DrawHLine(60, 0, 240);
    GUI_DrawHLine(120, 0, 240);
    GUI_DrawHLine(180, 0, 240);
	GUI_DrawHLine(240, 0, 240);

    GUI_DispStringHCenterAt("A: SNAKE", 120, 22);
    GUI_DispStringHCenterAt("B: BRICK", 120, 82);
    GUI_DispStringHCenterAt("C: FLAPPY", 120, 142);
    GUI_DispStringHCenterAt("D: 2048", 120, 202);
	GUI_DispStringHCenterAt("*: GESTURE", 120, 262);
}

#define APP_MAIN_STK_SZ (1024U)
uint64_t app_main_stk[APP_MAIN_STK_SZ / 8];
const osThreadAttr_t app_main_attr = {
  .stack_mem  = &app_main_stk[0],
  .stack_size = sizeof(app_main_stk)
};

__NO_RETURN void app_main (void *argument) {
  int16_t tX = 0, tY = 0;
  char buf[30];

  (void)argument;

  /* ---------------------------------------------------------
     INITIALIZATION ORDER IS CRITICAL
     1. Touch (I2C) - Independent of LCD pins
     2. GUI (LCD)   - Uses FSMC, might take over PF0-PF5
     3. Keypad      - Must be LAST to reclaim PF0-PF5 for keys
     --------------------------------------------------------- */
  Touch_Init();               // Initialize Touch Sensor
  
  
  GUI_Init();                 // Initialize Display
  
  Keypad_Init();              // Initialize Keypad (Reclaim GPIO F)

  DrawMainMenu();

  while (1) {
    /* Get Inputs using the abstraction in input.c */
    char key = Keypad_Get_Key();
    int isTouched = Touch_GetCoord(&tX, &tY);

    /* --- DEBUG: View Coords --- */
    if (isTouched) {
        GUI_SetColor(GUI_YELLOW);
        GUI_SetFont(GUI_FONT_13_1);
        // Print raw coords to verify calibration
        sprintf(buf, "X:%d Y:%d   ", tX, tY); 
        GUI_DispStringAt(buf, 0, 0);
    }

    /* --- MENU SELECTION LOGIC --- */
    
    // Zone 1: Top (Snake)
    if (key == 'A' || (isTouched && tY < 60)) {
        StartSnakeGame();
        DrawMainMenu();
        // Clear any touch that happened during game exit
        while(Touch_GetCoord(&tX, &tY)); 
    }
    // Zone 2: Middle-Top (Brick)
    else if (key == 'B' || (isTouched && tY >= 60 && tY < 120)) {
        StartBrickGame();
        DrawMainMenu();
        while(Touch_GetCoord(&tX, &tY));
    }
    // Zone 3: Middle-Bottom (Flappy)
    else if (key == 'C' || (isTouched && tY >= 120 && tY < 180)) {
        StartFlappyGame();
        DrawMainMenu();
        while(Touch_GetCoord(&tX, &tY));
    }
    // Zone 4: Bottom (2048)
    else if (key == 'D' || (isTouched && tY >= 180 && tY < 240)) {
        Start2048Game();
        DrawMainMenu();
        while(Touch_GetCoord(&tX, &tY));
    }
		 else if (key == '*' || (isTouched && tY >= 240)) {
        StartSwipeCheck();
        DrawMainMenu();
        while(Touch_GetCoord(&tX, &tY));
    }

    GUI_Delay(50);
  }
}