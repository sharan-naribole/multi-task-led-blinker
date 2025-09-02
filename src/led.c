/**
* @file led.c
* @author sharan-naribole
* @brief Minimal GPIO driver for on‑board LEDs (STM32F407G‑DISC1).
*/


#include "led.h"

// Keep register access explicit and readable.
#define REG32(addr) (*(volatile uint32_t *)(addr))


// --- RCC / GPIO base addresses (RM0090) --------------------------------------
#define PERIPH_BASE 0x40000000UL
#define AHB1PERIPH_BASE (PERIPH_BASE + 0x00020000UL)
#define RCC_BASE (AHB1PERIPH_BASE + 0x3800UL)
#define GPIOD_BASE (AHB1PERIPH_BASE + 0x0C00UL)


// --- Registers used -----------------------------------------------------------
#define RCC_AHB1ENR REG32(RCC_BASE + 0x30UL)
#define GPIOD_MODER REG32(GPIOD_BASE + 0x00UL)
#define GPIOD_ODR REG32(GPIOD_BASE + 0x14UL)


// --- Bits / masks -------------------------------------------------------------
#define RCC_AHB1ENR_GPIODEN_Pos 3U
#define RCC_AHB1ENR_GPIODEN_Msk (1UL << RCC_AHB1ENR_GPIODEN_Pos)


static inline uint32_t pin_mode_mask(uint8_t pin)
{
// Each MODER field is 2 bits per pin.
return (0x3UL << (2U * pin));
}


void led_delay(uint32_t count)
{
for(volatile uint32_t i = 0; i < count; ++i) { __asm volatile ("nop"); }
}


void led_init_all(void)
{
// Enable clock for GPIOD
RCC_AHB1ENR |= RCC_AHB1ENR_GPIODEN_Msk;


// Configure PD12..PD15 as general purpose outputs (01b)
// Clear then set to avoid accidental AF/AN modes.
uint32_t moder = GPIOD_MODER;
moder &= ~pin_mode_mask(LED_GREEN);
moder &= ~pin_mode_mask(LED_ORANGE);
moder &= ~pin_mode_mask(LED_RED);
moder &= ~pin_mode_mask(LED_BLUE);


moder |= (0x1UL << (2U * LED_GREEN));
moder |= (0x1UL << (2U * LED_ORANGE));
moder |= (0x1UL << (2U * LED_RED));
moder |= (0x1UL << (2U * LED_BLUE));


GPIOD_MODER = moder;


// Start all LEDs OFF
led_off(LED_GREEN);
led_off(LED_ORANGE);
led_off(LED_RED);
led_off(LED_BLUE);
}


void led_on(uint8_t led_no)
{
GPIOD_ODR |= (1UL << led_no);
}


void led_off(uint8_t led_no)
{
GPIOD_ODR &= ~(1UL << led_no);
}
