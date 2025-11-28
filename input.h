#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

// --- Keypad Definitions ---
void Keypad_Init(void);
char Keypad_Get_Key(void);

// --- Touch & Swipe Definitions ---

/* Swipe Direction Enum */
typedef enum {
    SWIPE_NONE = 0,
    SWIPE_LEFT,
    SWIPE_RIGHT,
    SWIPE_UP,
    SWIPE_DOWN
} Swipe_Dir_t;

void Touch_Init(void);
int  Touch_GetCoord(int16_t *x, int16_t *y);

/* Periodic State Machine for Swipe Detection */
Swipe_Dir_t Touch_Update_Swipe(void);

#endif