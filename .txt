#include "stm32f4xx.h"
#include "input.h"

/* Keymap 4×4 */
static const char keyMap[4][4] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};

void Keypad_Init(void)
{
    /* Enable GPIOF clock */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOFEN;

    /******** ROWS PF0–PF3 = OUTPUT ********/
    GPIOF->MODER &= ~(0xFF);      // clear PF0..PF3
    GPIOF->MODER |=  (0x55);      // set PF0..PF3 = output mode (01 each)

    /* Set rows HIGH initially (inactive) */
    GPIOF->BSRR = 0x0F;

    /******** COLS PF4–PF7 = INPUT + PULL-UP ********/
    GPIOF->MODER &= ~( (3<<8) | (3<<10) | (3<<12) | (3<<14) );   // input mode

    GPIOF->PUPDR &= ~( (3<<8) | (3<<10) | (3<<12) | (3<<14) );
    GPIOF->PUPDR |=  ( (1<<8) | (1<<10) | (1<<12) | (1<<14) );   // pull-up
}

static inline void delay_small(void)
{
    for (volatile int i = 0; i < 3000; i++);
}

char Keypad_Get_Key(void)
{
    for (int row = 0; row < 4; row++)
    {
        /******** DRIVE ROW r LOW, OTHERS HIGH ********/
        // Set all 4 rows HIGH
        GPIOF->BSRR = 0x0F;

        // Drive only row 'r' LOW (upper half of BSRR resets)
        GPIOF->BSRR = (1 << (row + 16));

        delay_small();

        /******** READ COLUMNS PF4–PF7 ********/
        uint32_t id = GPIOF->IDR;

        int c0 = !(id & (1<<4));
        int c1 = !(id & (1<<5));
        int c2 = !(id & (1<<6));
        int c3 = !(id & (1<<7));

        if (c0) return keyMap[row][0];
        if (c1) return keyMap[row][1];
        if (c2) return keyMap[row][2];
        if (c3) return keyMap[row][3];
    }

    return 0;   // No key pressed
}
