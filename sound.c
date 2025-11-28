#include "sound.h"
#include "stm32f4xx.h"
#include "Driver_I2C.h"
#include <math.h>

/* CS43L22 I2C Address */
#define CODEC_I2C_ADDR 0x4A 

extern ARM_DRIVER_I2C Driver_I2C1; 

static void CS43L22_Write(uint8_t reg, uint8_t val) {
    uint8_t data[2] = {reg, val};
    Driver_I2C1.MasterTransmit(CODEC_I2C_ADDR, data, 2, false);
    while(Driver_I2C1.GetStatus().busy); 
}

static void Audio_Reset_Init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    GPIOD->MODER &= ~(3U << (4 * 2)); 
    GPIOD->MODER |=  (1U << (4 * 2)); // PD4 Output

    GPIOD->BSRR = (1 << (4 + 16)); // PD4 Low (Reset)
    for(volatile int i=0; i<5000; i++); 
    GPIOD->BSRR = (1 << 4);        // PD4 High
}

static void I2S3_Init(void) {
    /* 1. Enable Clocks */
    RCC->APB1ENR |= RCC_APB1ENR_SPI3EN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN;

    /* 2. Configure GPIO AF06 (I2S3) */
    // PA4 (WS)
    GPIOA->MODER &= ~(3U << (4*2)); GPIOA->MODER |= (2U << (4*2));
    GPIOA->AFR[0] &= ~(0xF << (4*4)); GPIOA->AFR[0] |= (0x6 << (4*4));

    // PC7 (MCLK), PC10 (SCK), PC12 (SD)
    GPIOC->MODER &= ~((3U<<(7*2)) | (3U<<(10*2)) | (3U<<(12*2)));
    GPIOC->MODER |=  ((2U<<(7*2)) | (2U<<(10*2)) | (2U<<(12*2)));
    GPIOC->AFR[0] &= ~(0xF << (7*4));  GPIOC->AFR[0] |= (0x6 << (7*4));
    GPIOC->AFR[1] &= ~(0xF << (2*4));  GPIOC->AFR[1] |= (0x6 << (2*4));
    GPIOC->AFR[1] &= ~(0xF << (4*4));  GPIOC->AFR[1] |= (0x6 << (4*4));

    /* 3. Configure PLL for 48kHz */
    RCC->PLLI2SCFGR = (271 << 6) | (2 << 28); 
    RCC->CR |= RCC_CR_PLLI2SON; 
    while(!(RCC->CR & RCC_CR_PLLI2SRDY));

    /* 4. Configure SPI3 as I2S Master */
    SPI3->I2SCFGR = 0; 
    SPI3->I2SCFGR = SPI_I2SCFGR_I2SMOD | (2 << 4); // Master Tx, Philips
    SPI3->I2SPR = SPI_I2SPR_MCKOE | SPI_I2SPR_ODD | 4; // MCLK Output Enable
    SPI3->I2SCFGR |= SPI_I2SCFGR_I2SE;
}

void Sound_Init(void) {
    /* 1. Initialize I2C Driver (Used to be in Touch_Init) */
    Driver_I2C1.Initialize(NULL);
    Driver_I2C1.PowerControl(ARM_POWER_FULL);
    Driver_I2C1.Control(ARM_I2C_BUS_SPEED, ARM_I2C_BUS_SPEED_STANDARD);

    /* 2. Initialize Audio Reset Pin */
    Audio_Reset_Init();

    /* 3. Codec Config Sequence */
    CS43L22_Write(0x02, 0x01); // Power Down
    CS43L22_Write(0x04, 0xAF); // HP/Spk ON
    CS43L22_Write(0x05, 0x81); // Auto-detect clock
    CS43L22_Write(0x06, 0x04); // Slave, I2S 16-bit
    CS43L22_Write(0x20, 0xE6); // Volume A
    CS43L22_Write(0x21, 0xE6); // Volume B
    CS43L22_Write(0x02, 0x9E); // Power UP

    /* 4. Start I2S Data Stream */
    I2S3_Init();
}

void Audio_Beep(uint16_t frequency, uint32_t duration_ms) {
    int sample_rate = 48000;
    int samples = (sample_rate * duration_ms) / 1000;
    int16_t val;
    float t = 0;
    float dt = (float)frequency / sample_rate;

    for (int i = 0; i < samples; i++) {
        while (!(SPI3->SR & SPI_SR_TXE)); 
        
        val = (int16_t)(10000.0 * sinf(2.0f * 3.14159f * t));
        t += dt;
        if(t > 1.0f) t -= 1.0f;

        SPI3->DR = val; // Left
        while (!(SPI3->SR & SPI_SR_TXE)); 
        SPI3->DR = val; // Right
    }
}

void Sound_FruitBeep(void) {
    Audio_Beep(1046, 50); // C6, 50ms
}

void Sound_GameOverBeep(void) {
    Audio_Beep(261, 300); // C4, 300ms
    Audio_Beep(196, 500); // G3, 500ms
}