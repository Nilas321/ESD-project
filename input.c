#include "stm32f4xx.h"
#include "input.h" // Assuming this contains Keypad_Init, Keypad_Get_Key, Touch_Init, Touch_GetCoord declarations
#include "Driver_I2C.h"

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

    /******** ROWS PF0–PF3 = OUTPUT (Push-Pull) ********/
    GPIOF->MODER &= ~(0xFF);      // clear PF0..PF3
    GPIOF->MODER |=  (0x55);      // set PF0..PF3 = output mode (01 each)

    // FIX: Configure rows for Push-Pull output type (00)
    GPIOF->OTYPER &= ~(0x0F);

    // FIX: Set rows to Low Speed (00) - sufficient for keypad
    GPIOF->OSPEEDR &= ~(0xFF);

    /* Set rows HIGH initially (inactive) */
    GPIOF->BSRR = 0x0F;

    /******** COLS PF4–PF7 = INPUT + PULL-UP ********/
    GPIOF->MODER &= ~( (3<<8) | (3<<10) | (3<<12) | (3<<14) );   // input mode (00)

    GPIOF->PUPDR &= ~( (3<<8) | (3<<10) | (3<<12) | (3<<14) );
    GPIOF->PUPDR |=  ( (1<<8) | (1<<10) | (1<<12) | (1<<14) );   // pull-up (01)
}

static inline void delay_small(void)
{
    // Simple software delay for basic debouncing/stabilization
    for (volatile int i = 0; i < 3000; i++);
}

char Keypad_Get_Key(void)
{
    for (int row = 0; row < 4; row++)
    {
        /******** DRIVE ROW r LOW, OTHERS HIGH ********/
        // Set all 4 rows HIGH (Upper half BSRR: BSRR[3:0] = 1, BSRR[19:16] = 0)
        GPIOF->BSRR = 0x0F;

        // Drive only row 'r' LOW (Lower half BSRR: BSRR[19:16] = 1, BSRR[3:0] = 0)
        // BSRR[n] = 1 sets pin n high. BSRR[n+16] = 1 resets pin n low.
        GPIOF->BSRR = (1 << (row + 16));

        delay_small();

        /******** READ COLUMNS PF4–PF7 ********/
        uint32_t id = GPIOF->IDR;

        // Key pressed means the column line is pulled LOW by the row line
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

/* =========================================================================
    TOUCHSCREEN SECTION (STMPE811 via I2C1)
    ========================================================================= */

#define STMPE811_ADDR             0x41
#define STMPE811_REG_SYS_CTRL1    0x03
#define STMPE811_REG_SYS_CTRL2    0x04
#define STMPE811_REG_TSC_CTRL     0x40
#define STMPE811_REG_TSC_CFG      0x41
#define STMPE811_REG_FIFO_STA     0x4B
#define STMPE811_REG_FIFO_SIZE    0x4C
#define STMPE811_REG_TSC_DATA_X   0x4D

extern ARM_DRIVER_I2C Driver_I2C1;

static void STMPE811_Write(uint8_t reg, uint8_t val) {
    uint8_t data[2] = {reg, val};
    Driver_I2C1.MasterTransmit(STMPE811_ADDR, data, 2, false);
    // Safety check for asynchronous driver
    while(Driver_I2C1.GetStatus().busy);
}

static uint8_t STMPE811_Read(uint8_t reg) {
    uint8_t val = 0;
    // Transmit register address with repeated start (true)
    Driver_I2C1.MasterTransmit(STMPE811_ADDR, &reg, 1, true);
    // Safety check for asynchronous driver
    while(Driver_I2C1.GetStatus().busy); 
    
    // Receive data
    Driver_I2C1.MasterReceive(STMPE811_ADDR, &val, 1, false);
    // Safety check for asynchronous driver
    while(Driver_I2C1.GetStatus().busy); 
    
    return val;
}

void Touch_Init(void) {
    // These functions must be fully implemented and working in the I2C driver source
    Driver_I2C1.Initialize(NULL);
    Driver_I2C1.PowerControl(ARM_POWER_FULL);
    Driver_I2C1.Control(ARM_I2C_BUS_SPEED, ARM_I2C_BUS_SPEED_STANDARD); // Ensure speed is set

    STMPE811_Write(STMPE811_REG_SYS_CTRL1, 0x02); // Soft Reset
    delay_small();
    STMPE811_Write(STMPE811_REG_SYS_CTRL1, 0x00);

    STMPE811_Write(STMPE811_REG_SYS_CTRL2, 0x0C); // Enable TSC/ADC Clock
    STMPE811_Write(STMPE811_REG_TSC_CFG, 0xC4);    // Average 8 samples
    STMPE811_Write(STMPE811_REG_TSC_CTRL, 0x01);  // XYZ Mode enable
    STMPE811_Write(STMPE811_REG_FIFO_STA, 0x01);  // Reset FIFO
    STMPE811_Write(STMPE811_REG_FIFO_STA, 0x00);
}

int Touch_GetCoord(int16_t *x, int16_t *y) {
    uint8_t ctrl = STMPE811_Read(STMPE811_REG_TSC_CTRL);

    // Check Touch Det (Bit 7 of TSC_CTRL) & Data Available
    if ((ctrl & 0x80) && (STMPE811_Read(STMPE811_REG_FIFO_SIZE) > 0)) {
        uint8_t data[4];
        uint8_t reg = STMPE811_REG_TSC_DATA_X;

        // Transmit register address with repeated start
        Driver_I2C1.MasterTransmit(STMPE811_ADDR, &reg, 1, true);
        while(Driver_I2C1.GetStatus().busy); 
        
        // Receive 4 bytes of data (X-MSB, X-LSB, Y-MSB, Y-LSB)
        Driver_I2C1.MasterReceive(STMPE811_ADDR, data, 4, false);
        while(Driver_I2C1.GetStatus().busy); 

        // Clear FIFO to prevent old data buildup
        STMPE811_Write(STMPE811_REG_FIFO_STA, 0x01);
        STMPE811_Write(STMPE811_REG_FIFO_STA, 0x00);

        uint16_t rawX = (data[0] << 8) | data[1]; // D0 is MSB, D1 is LSB
        uint16_t rawY = (data[2] << 8) | data[3]; // D2 is MSB, D3 is LSB

        /* CALIBRATION (240x320 Portrait Display) */
        // Note: Using 32-bit cast to prevent overflow during multiplication
        uint32_t calX = ((uint32_t)rawX * 240) / 4096;
        uint32_t calY = ((uint32_t)rawY * 320) / 4096;

        // Invert Y axis to match display orientation (assuming raw Y increases downward)
        *y = 320 - calY;
        *x = calX;

        // Boundary Checks
        if (*y < 0) *y = 0;
        if (*y > 320) *y = 320;
        if (*x < 0) *x = 0;
        if (*x > 240) *x = 240;

        return 1;
    }
    return 0;   // No touch event
}