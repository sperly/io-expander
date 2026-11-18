/*
 * USART1 on PA9/PA10 (remapped onto pads PA11/PA12) driving the MAX3160.
 *
 * - Frame end detected with the USART receiver timeout (Modbus 3.5 character
 *   silence, at least 1.75 ms above 19200 baud).
 * - Transmit: DE high, receiver disabled (drops the RS-485 echo of the
 *   always-enabled MAX3160 receiver), TXE interrupt feeds bytes, TC
 *   interrupt releases DE and re-enables the receiver.
 * - In RS-232 mode DE only toggles the unused T2OUT output - harmless.
 */
#include "board.h"
#include "config.h"
#include "gpio.h"
#include "modbus_rtu.h"
#include "platform.h"

static uint8_t s_rx[MODBUS_MAX_ADU];
static volatile size_t s_rx_len;
static volatile bool s_rx_ready, s_rx_overflow;
static const uint8_t *s_tx;
static volatile size_t s_tx_len;
static volatile bool s_tx_busy;

void uart_init(uint32_t baud, uint8_t parity)
{
    RCC->APBENR2 |= RCC_APBENR2_USART1EN;
    SYSCFG->CFGR1 |= SYSCFG_CFGR1_PA11_RMP | SYSCFG_CFGR1_PA12_RMP; /* PA9/PA10 on pins 16/17 */
    gpio_af(GPIOA, PIN_UART_TX, 1);
    gpio_af(GPIOA, PIN_UART_RX, 1);
    GPIOA->PUPDR |= 1u << (2u * PIN_UART_RX); /* pull-up on RX */

    USART1->CR1 = 0;
    USART1->BRR = (SYSCLK_HZ + baud / 2u) / baud;
    uint32_t cr1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE_RXFNEIE | USART_CR1_RTOIE;
    unsigned char_bits = 10;
    if (parity != CFG_PARITY_NONE) {
        cr1 |= USART_CR1_M0 | USART_CR1_PCE; /* 8 data + parity = 9-bit word */
        if (parity == CFG_PARITY_ODD)
            cr1 |= USART_CR1_PS;
        char_bits = 11;
    }
    /* receiver timeout in bit times: 3.5 characters, min 1.75 ms */
    uint32_t rto = (char_bits * 7u + 1u) / 2u;
    uint32_t rto_min = (baud * 175u + 99999u) / 100000u;
    if (rto < rto_min)
        rto = rto_min;
    USART1->RTOR = rto;
    USART1->CR2 = USART_CR2_RTOEN;
    USART1->CR3 = USART_CR3_OVRDIS;
    USART1->CR1 = cr1;
    USART1->CR1 |= USART_CR1_UE;

    NVIC_SetPriority(USART1_IRQn, 0);
    NVIC_EnableIRQ(USART1_IRQn);
}

void USART1_IRQHandler(void)
{
    uint32_t isr = USART1->ISR;
    if (isr & (USART_ISR_PE | USART_ISR_FE | USART_ISR_NE)) {
        USART1->ICR = USART_ICR_PECF | USART_ICR_FECF | USART_ICR_NECF;
        s_rx_overflow = true; /* corrupt frame: drop it at frame end */
    }
    if (isr & USART_ISR_RXNE_RXFNE) {
        uint8_t b = (uint8_t)USART1->RDR;
        if (!s_rx_ready) {
            if (s_rx_len < sizeof(s_rx))
                s_rx[s_rx_len++] = b;
            else
                s_rx_overflow = true;
        }
    }
    if (isr & USART_ISR_RTOF) {
        USART1->ICR = USART_ICR_RTOCF;
        if (!s_rx_ready && s_rx_len) {
            if (s_rx_overflow)
                s_rx_len = 0;
            else
                s_rx_ready = true;
        }
        s_rx_overflow = false;
    }
    if ((USART1->CR1 & USART_CR1_TXEIE_TXFNFIE) && (isr & USART_ISR_TXE_TXFNF)) {
        if (s_tx_len) {
            USART1->TDR = *s_tx++;
            s_tx_len--;
        } else {
            USART1->CR1 &= ~USART_CR1_TXEIE_TXFNFIE;
            USART1->CR1 |= USART_CR1_TCIE;
        }
    }
    if ((USART1->CR1 & USART_CR1_TCIE) && (isr & USART_ISR_TC)) {
        USART1->CR1 &= ~USART_CR1_TCIE;
        USART1->ICR = USART_ICR_TCCF;
        gpio_clr(PIN_SER_DE_PORT, PIN_SER_DE);
        USART1->CR1 |= USART_CR1_RE;
        s_tx_busy = false;
    }
}

size_t uart_frame(const uint8_t **data)
{
    if (!s_rx_ready)
        return 0;
    *data = s_rx;
    return s_rx_len;
}

void uart_release_frame(void)
{
    __disable_irq();
    s_rx_len = 0;
    s_rx_ready = false;
    __enable_irq();
}

void uart_send(const uint8_t *data, size_t len)
{
    if (!len)
        return;
    while (s_tx_busy) {
    }
    s_tx = data;
    s_tx_len = len;
    s_tx_busy = true;
    USART1->CR1 &= ~USART_CR1_RE; /* ignore our own echo */
    gpio_set(PIN_SER_DE_PORT, PIN_SER_DE);
    USART1->CR1 |= USART_CR1_TXEIE_TXFNFIE;
}

bool uart_tx_busy(void) { return s_tx_busy; }
