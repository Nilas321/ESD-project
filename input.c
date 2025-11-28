#include "stm32f4xx.h"
#include "input.h"
#include "Driver_I2C.h"








/* Keymap */
static const char keyMap[4][4] = {
    {'1','2','3','A'}, // Row 1 (PC12)
    {'4','5','6','B'}, // Row 2 (PD2)
    {'7','8','9','C'}, // Row 3 (PH6)
    {'*','0','#','D'}  // Row 4 (PH7)
};

void Keypad_Init(void)
{
    /* 1. ENABLE CLOCKS: GPIOB, GPIOC, GPIOD, GPIOH */
    RCC->AHB1ENR |= (RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN | 
                     RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_GPIOHEN);

    /* 2. CONFIGURE ROWS (Outputs, Active Low) */
    
    // Row 1: PC12
    GPIOC->MODER &= ~(3 << (12*2));
    GPIOC->MODER |=  (1 << (12*2)); // Output
    GPIOC->BSRR   =  (1 << 12);     // High (Inactive)

    // Row 2: PD2
    GPIOD->MODER &= ~(3 << (2*2));
    GPIOD->MODER |=  (1 << (2*2));  // Output
    GPIOD->BSRR   =  (1 << 2);      // High (Inactive)

    // Row 3 & 4: PH6, PH7
    GPIOH->MODER &= ~((3<<(6*2)) | (3<<(7*2)));
    GPIOH->MODER |=  ((1<<(6*2)) | (1<<(7*2))); // Output
    GPIOH->BSRR   =  (1<<6) | (1<<7);           // High (Inactive)

    /* 3. CONFIGURE COLS (Inputs) */

    // Cols 1, 2, 3: PC8, PC9, PC10
    // Enable Internal Pull-Ups 
    GPIOC->MODER &= ~((3<<(8*2)) | (3<<(9*2)) | (3<<(10*2))); 
    GPIOC->PUPDR &= ~((3<<(8*2)) | (3<<(9*2)) | (3<<(10*2))); 
    GPIOC->PUPDR |=  ((1<<(8*2)) | (1<<(9*2)) | (1<<(10*2))); 

    // Col 4: PB15 (REPLACEMENT for PH14)
    // Configure PB15 as Input with Internal Pull-Up
    GPIOB->MODER &= ~(3 << (15*2)); // Input Mode
    GPIOB->PUPDR &= ~(3 << (15*2)); // Clear
    GPIOB->PUPDR |=  (1 << (15*2)); // Set Pull-Up (01)
}

static inline void delay_small(void)
{
    for (volatile int i=0; i<3000; i++);
}

char Keypad_Get_Key(void)
{
    for (int row = 0; row < 4; row++)
    {
        // 1. SET ALL ROWS HIGH (Inactive)
        GPIOC->BSRR = (1 << 12);
        GPIOD->BSRR = (1 << 2);
        GPIOH->BSRR = (1 << 6) | (1 << 7);

        // 2. DRIVE CURRENT ROW LOW (Active)
        if      (row == 0) GPIOC->BSRR = (1 << (12 + 16)); // PC12 Low
        else if (row == 1) GPIOD->BSRR = (1 << (2 + 16));  // PD2 Low
        else if (row == 2) GPIOH->BSRR = (1 << (6 + 16));  // PH6 Low
        else if (row == 3) GPIOH->BSRR = (1 << (7 + 16));  // PH7 Low

        delay_small();

        // 3. READ COLS (PC8, PC9, PC10, PB15)
        uint32_t idrC = GPIOC->IDR;
        
        // Active Low: Pressing key reads 0
        int c0 = !(idrC & (1<<8));         // Col 1: PC8
        int c1 = !(idrC & (1<<9));         // Col 2: PC9
        int c2 = !(idrC & (1<<10));        // Col 3: PC10
        int c3 = !(GPIOB->IDR & (1<<15));  // Col 4: PB15 (New)

        if (c0) return keyMap[row][0];
        if (c1) return keyMap[row][1];
        if (c2) return keyMap[row][2];
        if (c3) return keyMap[row][3];
    }
    return 0;
}








/* =========================================================================
   TOUCHSCREEN SECTION (STMPE811 via I2C1)
   ========================================================================= */

#define STMPE811_ADDR           0x41 
#define STMPE811_REG_SYS_CTRL1  0x03
#define STMPE811_REG_SYS_CTRL2  0x04
#define STMPE811_REG_TSC_CTRL   0x40
#define STMPE811_REG_TSC_CFG    0x41
#define STMPE811_REG_FIFO_STA   0x4B
#define STMPE811_REG_FIFO_SIZE  0x4C
#define STMPE811_REG_TSC_DATA_X 0x4D 

extern ARM_DRIVER_I2C Driver_I2C1; 

static void STMPE811_Write(uint8_t reg, uint8_t val) {
    uint8_t data[2] = {reg, val};
    Driver_I2C1.MasterTransmit(STMPE811_ADDR, data, 2, false);
    // Note: If driver is non-blocking, you might need a check here to wait for completion
    // e.g., while(Driver_I2C1.GetStatus().busy);
}

static uint8_t STMPE811_Read(uint8_t reg) {
    uint8_t val = 0;
    Driver_I2C1.MasterTransmit(STMPE811_ADDR, &reg, 1, true); // No Stop bit (Repeated Start)
    Driver_I2C1.MasterReceive(STMPE811_ADDR, &val, 1, false);
    // Note: Add busy wait here if using async driver
    return val;
}

void Touch_Init(void) {
    Driver_I2C1.Initialize(NULL);
    Driver_I2C1.PowerControl(ARM_POWER_FULL);
    Driver_I2C1.Control(ARM_I2C_BUS_SPEED, ARM_I2C_BUS_SPEED_STANDARD); // Ensure speed is set

    STMPE811_Write(STMPE811_REG_SYS_CTRL1, 0x02); // Soft Reset
    delay_small(); 
    STMPE811_Write(STMPE811_REG_SYS_CTRL1, 0x00);
    
    STMPE811_Write(STMPE811_REG_SYS_CTRL2, 0x0C); // Enable TSC/ADC Clock
    STMPE811_Write(STMPE811_REG_TSC_CFG, 0xC4);   // Average 8 samples
    STMPE811_Write(STMPE811_REG_TSC_CTRL, 0x01);  // XYZ Mode enable
    STMPE811_Write(STMPE811_REG_FIFO_STA, 0x01);  // Reset FIFO
    STMPE811_Write(STMPE811_REG_FIFO_STA, 0x00);
}

int Touch_GetCoord(int16_t *x, int16_t *y) {
    uint8_t ctrl = STMPE811_Read(STMPE811_REG_TSC_CTRL);
    
    // Check Touch Det (Bit 7) & Data Available
    if ((ctrl & 0x80) && (STMPE811_Read(STMPE811_REG_FIFO_SIZE) > 0)) {
        uint8_t data[4];
        uint8_t reg = STMPE811_REG_TSC_DATA_X;
        
        Driver_I2C1.MasterTransmit(STMPE811_ADDR, &reg, 1, true);
        Driver_I2C1.MasterReceive(STMPE811_ADDR, data, 4, false);

        // Clear FIFO to prevent old data buildup
        STMPE811_Write(STMPE811_REG_FIFO_STA, 0x01);
        STMPE811_Write(STMPE811_REG_FIFO_STA, 0x00);

        uint16_t rawX = (data[0] << 8) | data[1];
        uint16_t rawY = (data[2] << 8) | data[3];

        /* CALIBRATION (240x320) */
        // Note: Using 32-bit cast to prevent overflow during multiplication
        uint32_t calX = ((uint32_t)rawX * 240) / 4096;
        uint32_t calY = ((uint32_t)rawY * 320) / 4096;

        *y = 320 - calY; 
        *x = calX;

        // Boundary Checks
        if (*y < 0) *y = 0;
        if (*y > 320) *y = 320;
        if (*x < 0) *x = 0;
        if (*x > 240) *x = 240;
        
        return 1; 
    }
    return 0; 
}