#include "input.h"
#include "stm32f4xx.h"
#include "Driver_I2C.h"
#include <stdlib.h> // For abs()

/* =========================================================================
   KEYPAD SECTION (GPIO PORT C)
   Rows: PC0-PC3 (Output), Cols: PC4-PC7 (Input Pull-up)
   ========================================================================= */

static const char keyMap[4][4] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};

static void delay_small(void)
{
    // usage of volatile prevents compiler optimization removal
    for (volatile int i = 0; i < 3000; i++) { __NOP(); } 
}

void Keypad_Init(void)
{
    /* 1. Enable GPIOC clock */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;

    /* 2. Configure ROWS (PC0–PC3) as OUTPUT */
    GPIOC->MODER &= ~(0x000000FF);      // Clear bits for Pin 0-3
    GPIOC->MODER |=  (0x00000055);      // Set bits (01) for Pin 0-3 (Output)
    
    // Set Rows HIGH (Idle state)
    GPIOC->BSRR = 0x0000000F;

    /* 3. Configure COLS (PC4–PC7) as INPUT with PULL-UP */
    GPIOC->MODER &= ~(0x0000FF00);      // Clear bits for Pin 4-7 (Input is 00)
    
    GPIOC->PUPDR &= ~(0x0000FF00);      // Clear pull-up/down bits
    GPIOC->PUPDR |=  (0x00005500);      // Set bits (01) for Pin 4-7 (Pull-up)
}

char Keypad_Get_Key(void)
{
    for (int row = 0; row < 4; row++)
    {
        /* DRIVE CURRENT ROW LOW, OTHERS HIGH */
        GPIOC->BSRR = 0x0000000F;           // Set all rows HIGH first
        GPIOC->BSRR = (1 << (row + 16));    // Reset (Low) current row (Bits 16-31)

        delay_small();   // Wait for electrical stabilization

        /* READ COLUMNS (PC4–PC7) */
        uint32_t idr = GPIOC->IDR;

        // Check for LOW state (Active Low)
        if (!(idr & (1<<4))) return keyMap[row][0];
        if (!(idr & (1<<5))) return keyMap[row][1];
        if (!(idr & (1<<6))) return keyMap[row][2];
        if (!(idr & (1<<7))) return keyMap[row][3];
    }
    return 0; // No key pressed
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

typedef enum {
    STATE_IDLE,
    STATE_TRACKING,
    STATE_LOCKED     // Swipe detected, waiting for release
} Swipe_State_t;

#define SWIPE_THRESHOLD  40  // Minimum pixels to count as a swipe

Swipe_Dir_t Touch_Update_Swipe(void) {
    // Persistent state variables
    static Swipe_State_t state = STATE_IDLE;
    static int16_t start_x = 0, start_y = 0;
    
    int16_t curr_x, curr_y;
    Swipe_Dir_t detected_swipe = SWIPE_NONE;

    // 1. Poll Hardware
    int touch_active = Touch_GetCoord(&curr_x, &curr_y);

    // 2. State Machine
    switch (state) {
        
        case STATE_IDLE:
            if (touch_active) {
                // New touch started
                start_x = curr_x;
                start_y = curr_y;
                state = STATE_TRACKING;
            }
            break;

        case STATE_TRACKING:
            if (touch_active) {
                int16_t diff_x = curr_x - start_x;
                int16_t diff_y = curr_y - start_y;

                // Check absolute distance
                if (abs(diff_x) > SWIPE_THRESHOLD || abs(diff_y) > SWIPE_THRESHOLD) {
                    
                    // Determine dominant axis
                    if (abs(diff_x) > abs(diff_y)) {
                        // Horizontal Swipe
                        detected_swipe = (diff_x > 0) ? SWIPE_RIGHT : SWIPE_LEFT;
                    } else {
                        // Vertical Swipe
                        // Note: Ensure this matches your screen orientation logic
                        detected_swipe = (diff_y > 0) ? SWIPE_UP : SWIPE_DOWN;
                    }

                    // LOCK the state so we don't double-trigger or detect lift-off noise
                    state = STATE_LOCKED;
                }
            } else {
                // User let go before crossing threshold (Tap or jitter)
                state = STATE_IDLE;
            }
            break;

        case STATE_LOCKED:
            if (!touch_active) {
                // User finally lifted finger, reset for next interaction
                state = STATE_IDLE;
            }
            // While touch is still active here, we return SWIPE_NONE 
            // and ignore all movement (thumb wobble).
            break;
    }

    return detected_swipe;
}