#include "stm32f4xx.h"
#include "Driver_I2C.h"
#include "CS42L52.h"

#define CODEC_I2C_ADDR 0x4A  // 

extern ARM_DRIVER_I2C Driver_I2C1; // Reuse the I2C driver from Touchscreen

// -----------------------------------------------------------
// 1. Low-Level I2C Functions (Reused from Touchscreen logic)
// -----------------------------------------------------------
static void Codec_Write(uint8_t reg, uint8_t val) {
    uint8_t data[2] = {reg, val};
    Driver_I2C1.MasterTransmit(CODEC_I2C_ADDR, data, 2, false);
    while(Driver_I2C1.GetStatus().busy); // Blocking wait 
}

// -----------------------------------------------------------
// 2. Master Clock (MCLK) Generation using TIM3 on PC6
// -----------------------------------------------------------
void MCLK_Init(void) {
    // Enable TIM3 and GPIOC clocks
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN; 
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;

    // Configure PC6 as Alternate Function (AF2) for TIM3
    GPIOC->MODER &= ~(3U << (6 * 2));
    GPIOC->MODER |=  (2U << (6 * 2)); // AF Mode
    GPIOC->AFR[0] &= ~(0xF << (6 * 4));
    GPIOC->AFR[0] |=  (2U << (6 * 4)); // AF2

    // Configure TIM3 for PWM generation (~12MHz required)
    // Settings based on SystemCoreClock 168MHz to get correct MCLK
    TIM3->CCMR1 = 0x60;       // PWM Mode 1
    TIM3->ARR   = 1;          // Auto-reload
    TIM3->CCR1  = 1;          // 50% Duty Cycle
    TIM3->CCER  |= 1;         // Enable Capture/Compare 1
    TIM3->CR1   |= 1;         // Enable Timer 
}

// -----------------------------------------------------------
// 3. Codec Initialization Sequence
// -----------------------------------------------------------
void Codec_Init(void) {
    // 1. Generate MCLK first (Required for Codec internal logic)
    MCLK_Init(); 

    // 2. Power Down Codec during config
    Codec_Write(CS42L52_PWRCTL1, 0x99); 
    
    // 3. Configure Interface 
    // 0x80 = I2S Slave Mode (Beep Gen only needs this)
    Codec_Write(CS42L52_IFACE_CTL1, 0x80); 
    
    // 4. Set Volume (0x00 is 0dB/Max, 0x18 is +12dB)
    Codec_Write(CS42L52_MASTERA_VOL, 0x00); 
    
    // 5. Power Up
    Codec_Write(CS42L52_PWRCTL1, 0x00); // [cite: 1705]
}

// -----------------------------------------------------------
// 4. Beep Function
// -----------------------------------------------------------
void Codec_Beep(uint8_t pitch, uint8_t duration) {
    // Set Volume for Beep
    Codec_Write(CS42L52_BEEP_VOL, 0x06); // -6dB
    
    // Set Frequency and Duration
    Codec_Write(CS42L52_BEEP_FREQ, pitch | duration);
    
    // Trigger Single Beep
    Codec_Write(CS42L52_BEEP_TONE_CTL, 0x40);
    
    // Note: To stop/reset, you would write 0x00 to TONE_CTL [cite: 1707]
}