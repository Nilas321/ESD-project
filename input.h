#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

// --- Keypad Definitions ---
void Keypad_Init(void);
char Keypad_Get_Key(void);

// --- Touch & Swipe Definitions ---

void Touch_Init(void);
int  Touch_GetCoord(int16_t *x, int16_t *y);
#endif