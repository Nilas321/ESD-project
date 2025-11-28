#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

/* Initialize Audio Codec and I2S Interface */
void Sound_Init(void);

/* Play a short beep (e.g., when eating fruit) */
void Sound_FruitBeep(void);

/* Play a game over sound */
void Sound_GameOverBeep(void);

/* Core beep function: Frequency in Hz, Duration in ms */
void Audio_Beep(uint16_t frequency, uint32_t duration_ms);

#endif