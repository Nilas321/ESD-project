#include "stm32f4xx.h"
#include "sound.h"
#include <stdint.h>

#include "cmsis_os2.h"

/* Buzzer pin = PB4 */
#define BUZ_PIN     (1 << 4)

void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms * 16000; i++) __NOP();
}

void Sound_Init(void)
{
    /* Enable GPIOB clock */
    RCC->AHB1ENR |= (1 << 1);

    /* Set PB4 as GENERAL PURPOSE OUTPUT (01) */
    GPIOB->MODER &= ~(3 << (4 * 2));
    GPIOB->MODER |=  (1 << (4 * 2));

    /* Push-pull, no pull-up/down */
    GPIOB->OTYPER &= ~BUZ_PIN;
    GPIOB->PUPDR   &= ~(3 << (4 * 2));

    /* Start with buzzer OFF */
    GPIOB->BSRR = (BUZ_PIN << 16);   // reset PB4
}

static void beep_ms(uint32_t time_ms)
{
    GPIOB->BSRR = BUZ_PIN;         // PB4 HIGH = 3.3V ? buzzer ON
    delay_ms(time_ms);
    GPIOB->BSRR = (BUZ_PIN << 16); // PB4 LOW  ? buzzer OFF
}

/********** SHORT BEEP FOR FRUIT **********/
void Sound_FruitBeep(void)
{
    beep_ms(45);   // small chirp
}

/********** LONG BEEP FOR GAME OVER **********/
void Sound_GameOverBeep(void)
{
    beep_ms(200);
    osDelay(50);
    beep_ms(200);	
	  osDelay(50);
	  beep_ms(200);
	
}
