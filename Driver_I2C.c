#include "Driver_I2C.h"
#include "stm32f4xx.h"

/* =========================================
   1. CONFIG / CONSTANTS
   ========================================= */

#define I2C_TIMEOUT      (100000U)
// APB1 Clock assumption: 42 MHz
#define I2C_PCLK1_MHZ    42U
#define I2C_CCR_SM_100K  210U
#define I2C_TRISE_SM     43U

/* =========================================
   2. BARE METAL HELPER (I2C1 Specific)
   ========================================= */

static void HW_I2C1_Init(void) {
    // 1) Enable Clocks for I2C1 and GPIOB
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    (void)RCC->APB1ENR; // Short delay

    // 2) Configure PB8 (SCL) and PB9 (SDA) as AF4, Open-Drain, Pull-Up, High Speed
    // Clear Mode, Speed, Pull-up, Output Type fields for Pin 8 and 9
    GPIOB->MODER   &= ~((3U << (8U * 2U)) | (3U << (9U * 2U)));
    GPIOB->OSPEEDR &= ~((3U << (8U * 2U)) | (3U << (9U * 2U)));
    GPIOB->PUPDR   &= ~((3U << (8U * 2U)) | (3U << (9U * 2U)));
    GPIOB->OTYPER  &= ~((1U << 8U) | (1U << 9U));

    // Set new values
    GPIOB->MODER   |=  ((2U << (8U * 2U)) | (2U << (9U * 2U))); // AF Mode
    GPIOB->OTYPER  |=  ((1U << 8U) | (1U << 9U));               // Open Drain
    GPIOB->OSPEEDR |=  ((3U << (8U * 2U)) | (3U << (9U * 2U))); // High Speed
    GPIOB->PUPDR   |=  ((1U << (8U * 2U)) | (1U << (9U * 2U))); // Pull-Up

    // Set AF4 (I2C1) for PB8 and PB9. AFR[1] is for pins 8-15
    GPIOB->AFR[1]  &= ~((0xFU << ((8U - 8U) * 4U)) | (0xFU << ((9U - 8U) * 4U)));
    GPIOB->AFR[1]  |=  ((0x4U << ((8U - 8U) * 4U)) | (0x4U << ((9U - 8U) * 4U)));

    // 3) I2C1 Peripheral Configuration
    I2C1->CR1 &= ~I2C_CR1_PE; // Disable before config

    I2C1->CR1 |= I2C_CR1_SWRST; // Soft Reset
    I2C1->CR1 &= ~I2C_CR1_SWRST;

    I2C1->CR2 = (I2C_PCLK1_MHZ & 0x3FU); // Set Frequency
    I2C1->CCR = I2C_CCR_SM_100K;         // Standard Mode
    I2C1->TRISE = I2C_TRISE_SM;

    I2C1->CR1 |= I2C_CR1_PE; // Enable Peripheral
}

/* =========================================
   3. CMSIS-DRIVER INTERFACE
   ========================================= */

static ARM_I2C_SignalEvent_t cb_event_ptr = NULL;

static ARM_DRIVER_VERSION I2C_GetVersion(void) {
    ARM_DRIVER_VERSION ver = {ARM_I2C_API_VERSION, 1U};
    return ver;
}

static ARM_I2C_CAPABILITIES I2C_GetCapabilities(void) {
    ARM_I2C_CAPABILITIES caps;
    memset(&caps, 0, sizeof(caps));
    return caps;
}

static int32_t I2C_Initialize (ARM_I2C_SignalEvent_t cb_event) {
    cb_event_ptr = cb_event;
    HW_I2C1_Init(); 
    return ARM_DRIVER_OK;
}

static int32_t I2C_Uninitialize (void) {
    I2C1->CR1 &= ~I2C_CR1_PE;
    return ARM_DRIVER_OK;
}

static int32_t I2C_PowerControl (ARM_POWER_STATE state) {
    if (state == ARM_POWER_FULL) {
        I2C1->CR1 |= I2C_CR1_PE;
        return ARM_DRIVER_OK;
    } else if (state == ARM_POWER_OFF) {
        I2C1->CR1 &= ~I2C_CR1_PE;
        return ARM_DRIVER_OK;
    }
    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

static int32_t I2C_MasterTransmit (uint32_t addr, const uint8_t *data, uint32_t num, bool xfer_pending) {
    if ((data == NULL) || (num == 0U)) return ARM_DRIVER_ERROR_PARAMETER;
    
    uint32_t timeout;

    // Start
    I2C1->CR1 |= I2C_CR1_START;
    timeout = I2C_TIMEOUT;
    while (!(I2C1->SR1 & I2C_SR1_SB)) { if (--timeout == 0) return ARM_DRIVER_ERROR_TIMEOUT; }

    // Address (Write)
    I2C1->DR = (uint8_t)((addr << 1) & 0xFEU);
    timeout = I2C_TIMEOUT;
    while (!(I2C1->SR1 & I2C_SR1_ADDR)) { if (--timeout == 0) return ARM_DRIVER_ERROR_TIMEOUT; }
    (void)I2C1->SR2; 

    // Data
    for (uint32_t i = 0U; i < num; i++) {
        timeout = I2C_TIMEOUT;
        while (!(I2C1->SR1 & I2C_SR1_TXE)) { if (--timeout == 0) return ARM_DRIVER_ERROR_TIMEOUT; }
        I2C1->DR = data[i];
    }

    // Wait for BTF
    timeout = I2C_TIMEOUT;
    while (!(I2C1->SR1 & I2C_SR1_BTF)) { if (--timeout == 0) return ARM_DRIVER_ERROR_TIMEOUT; }

    // Stop
    if (!xfer_pending) {
        I2C1->CR1 |= I2C_CR1_STOP;
    }
    return ARM_DRIVER_OK;
}

static int32_t I2C_MasterReceive (uint32_t addr, uint8_t *data, uint32_t num, bool xfer_pending) {
    if ((data == NULL) || (num == 0U)) return ARM_DRIVER_ERROR_PARAMETER;
    
    uint32_t timeout;

    // Start
    I2C1->CR1 |= I2C_CR1_START;
    timeout = I2C_TIMEOUT;
    while (!(I2C1->SR1 & I2C_SR1_SB)) { if (--timeout == 0) return ARM_DRIVER_ERROR_TIMEOUT; }

    // Address (Read)
    I2C1->DR = (uint8_t)((addr << 1) | 0x01U);
    timeout = I2C_TIMEOUT;
    while (!(I2C1->SR1 & I2C_SR1_ADDR)) { if (--timeout == 0) return ARM_DRIVER_ERROR_TIMEOUT; }

    // Note: Use simple state machine for RX to handle NACK/STOP timing correctly
    if (num == 1U) {
        // 1 Byte Read
        I2C1->CR1 &= ~I2C_CR1_ACK;
        (void)I2C1->SR2;            // Clear ADDR
        I2C1->CR1 |= I2C_CR1_STOP;
        
        timeout = I2C_TIMEOUT;
        while (!(I2C1->SR1 & I2C_SR1_RXNE)) { if (--timeout == 0) return ARM_DRIVER_ERROR_TIMEOUT; }
        data[0] = (uint8_t)I2C1->DR;
        
    } else if (num == 2U) {
        // 2 Byte Read
        I2C1->CR1 |= I2C_CR1_POS;
        I2C1->CR1 &= ~I2C_CR1_ACK;
        (void)I2C1->SR2;            // Clear ADDR
        
        timeout = I2C_TIMEOUT;
        while (!(I2C1->SR1 & I2C_SR1_BTF)) { if (--timeout == 0) return ARM_DRIVER_ERROR_TIMEOUT; }
        
        I2C1->CR1 |= I2C_CR1_STOP;
        data[0] = (uint8_t)I2C1->DR;
        data[1] = (uint8_t)I2C1->DR;
        I2C1->CR1 &= ~I2C_CR1_POS;
        
    } else {
        // N > 2 Bytes
        I2C1->CR1 |= I2C_CR1_ACK;
        (void)I2C1->SR2;            // Clear ADDR
        
        uint32_t i = 0U;
        while (i < (num - 3U)) {
            timeout = I2C_TIMEOUT;
            while (!(I2C1->SR1 & I2C_SR1_RXNE)) { if (--timeout == 0) return ARM_DRIVER_ERROR_TIMEOUT; }
            data[i++] = (uint8_t)I2C1->DR;
        }
        // Wait for BTF (N-2 received, N-1 in shift register)
        timeout = I2C_TIMEOUT;
        while (!(I2C1->SR1 & I2C_SR1_BTF)) { if (--timeout == 0) return ARM_DRIVER_ERROR_TIMEOUT; }
        
        I2C1->CR1 &= ~I2C_CR1_ACK; // NACK
        data[i++] = (uint8_t)I2C1->DR; // Read N-2
        
        I2C1->CR1 |= I2C_CR1_STOP; // STOP
        data[i++] = (uint8_t)I2C1->DR; // Read N-1
        
        timeout = I2C_TIMEOUT;
        while (!(I2C1->SR1 & I2C_SR1_RXNE)) { if (--timeout == 0) return ARM_DRIVER_ERROR_TIMEOUT; }
        data[i++] = (uint8_t)I2C1->DR; // Read N
    }

    return ARM_DRIVER_OK;
}

// ... (Stubs for Slave/Control unchanged) ...
static int32_t I2C_SlaveTransmit (const uint8_t *data, uint32_t num) { (void)data; (void)num; return ARM_DRIVER_ERROR_UNSUPPORTED; }
static int32_t I2C_SlaveReceive (uint8_t *data, uint32_t num) { (void)data; (void)num; return ARM_DRIVER_ERROR_UNSUPPORTED; }
static int32_t I2C_GetDataCount (void) { return 0; }
static int32_t I2C_Control (uint32_t control, uint32_t arg) { (void)control; (void)arg; return ARM_DRIVER_OK; }
static ARM_I2C_STATUS I2C_GetStatus (void) { ARM_I2C_STATUS s = {0}; return s; }

ARM_DRIVER_I2C Driver_I2C1 = {
    I2C_GetVersion,
    I2C_GetCapabilities,
    I2C_Initialize,
    I2C_Uninitialize,
    I2C_PowerControl,
    I2C_MasterTransmit,
    I2C_MasterReceive,
    I2C_SlaveTransmit,
    I2C_SlaveReceive,
    I2C_GetDataCount,
    I2C_Control,
    I2C_GetStatus
};