#pragma once

// ==== General ====
#define BOARD_NAME          "Locket5"
#define APP_NAME            "HeartOfStormPlayer"

// MCU type as defined in the ST header.
#define STM32L151xB

// Freq of external crystal if any. Leave it here even if not used.
#define CRYSTAL_FREQ_HZ     12000000

// PortMinTim_t: GPIO, Pin, Tim, TimChnl, invInverted, omPushPull, TopValue
// UART
#define UART_GPIO       GPIOA
#define UART_TX_PIN     9
#define UART_RX_PIN     10

// LED
#define LED_EN_PIN      { GPIOB, 2, omPushPull }
#define LED_G_PIN       { GPIOB, 1, TIM3, 4, invInverted, omOpenDrain, 255 }
#define LED_B_PIN       { GPIOB, 0, TIM3, 3, invInverted, omOpenDrain, 255 }
#define LED_R_PIN       { GPIOB, 5, TIM3, 2, invInverted, omOpenDrain, 255 }

// Buttons
#define BTN1_PIN        GPIOA, 0, pudPullDown
#define BTN2_PIN        GPIOA, 1, pudPullDown
#define BTN3_PIN        GPIOB, 8, pudPullDown

// Vibro
#define VIBRO_SETUP     { GPIOB, 12, TIM10, 1, invNotInverted, omPushPull, 99 }

// Beeper
#define BEEPER_TOP      22
#define BEEPER_PIN      { GPIOB, 15, TIM11, 1, invNotInverted, omPushPull, BEEPER_TOP }

// DIP switch
#define DIP_SW_CNT      8
#define DIP_SW1         { GPIOB, 13, pudPullUp }
#define DIP_SW2         { GPIOB, 14, pudPullUp }
#define DIP_SW3         { GPIOA,  8, pudPullUp }
#define DIP_SW4         { GPIOA, 11, pudPullUp }
#define DIP_SW5         { GPIOA, 15, pudPullUp }
#define DIP_SW6         { GPIOA, 12, pudPullUp }
#define DIP_SW7         { GPIOC, 13, pudPullUp }
#define DIP_SW8         { GPIOC, 14, pudPullUp }

// I2C
#define I2C1_GPIO       GPIOB
#define I2C1_SCL        6
#define I2C1_SDA        7

// Pill power
#define PILL_PWR_PIN    { GPIOB, 3, omPushPull }

// Radio: SPI, PGpio, Sck, Miso, Mosi, Cs, Gdo0
#define CC_Setup0       SPI1, GPIOA, 5,6,7, GPIOA,4, GPIOA,3

#define I2C1_BAUDRATE   400000
#define I2C_PILL        i2c1

#define STM32_DMA_REQUIRED  TRUE
// ==== Uart ====
#define UART_DMA_TX_MODE(Chnl) (STM32_DMA_CR_CHSEL(Chnl) | DMA_PRIORITY_LOW | STM32_DMA_CR_MSIZE_BYTE | STM32_DMA_CR_PSIZE_BYTE | STM32_DMA_CR_MINC | STM32_DMA_CR_DIR_M2P | STM32_DMA_CR_TCIE)
#define UART_DMA_RX_MODE(Chnl) (STM32_DMA_CR_CHSEL(Chnl) | DMA_PRIORITY_MEDIUM | STM32_DMA_CR_MSIZE_BYTE | STM32_DMA_CR_PSIZE_BYTE | STM32_DMA_CR_MINC | STM32_DMA_CR_DIR_P2M | STM32_DMA_CR_CIRC)
#define UART_DMA_TX     STM32_DMA_STREAM_ID(1, 4)
#define UART_DMA_RX     STM32_DMA_STREAM_ID(1, 5)
#define UART_DMA_CHNL   0   // Dummy

#define I2C1_DMA_TX     STM32_DMA_STREAM_ID(1, 6)
#define I2C1_DMA_RX     STM32_DMA_STREAM_ID(1, 7)
#define I2C1_DMA_CHNL   0   // Dummy

#define PRINTF_FLOAT_EN FALSE
#define UART_TXBUF_SZ   256
#define UART_RXBUF_SZ   99

#define UARTS_CNT       1

#define CMD_UART_PARAMS \
    USART1, UART_GPIO, UART_TX_PIN, UART_GPIO, UART_RX_PIN, \
    UART_DMA_TX, UART_DMA_RX, UART_DMA_TX_MODE(UART_DMA_CHNL), UART_DMA_RX_MODE(UART_DMA_CHNL)
