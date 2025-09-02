/**
* @file led.h
* @author sharan-naribole
* @brief Minimal GPIO driver for on‑board LEDs (STM32F407G‑DISC1).
*
* Provides a small, self‑contained API used by the scheduler demo. The
* implementation uses direct register access (no HAL/CubeMX) to keep the
* code easy to follow for bare‑metal learning.
*/

#ifndef LED_H_
#define LED_H_

#include <stdint.h>
#include <stdbool.h>


// -----------------------------------------------------------------------------
// Board mapping (PD12..PD15)
// -----------------------------------------------------------------------------
#define LED_GREEN 12U
#define LED_ORANGE 13U
#define LED_RED 14U
#define LED_BLUE 15U


// Blink periods in SysTick ticks (1 kHz → 1 tick = 1 ms)
#define LED_GREEN_FREQ 1000U
#define LED_ORANGE_FREQ 500U
#define LED_BLUE_FREQ 250U
#define LED_RED_FREQ 125U


// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------
/**
* @brief Enable GPIO clocks and configure pins PD12..PD15 as push‑pull outputs.
* All LEDs are turned OFF after initialization.
*/
void led_init_all(void);


/**
* @brief Turn ON the selected LED (pin number on port D).
*/
void led_on(uint8_t led_no);


/**
* @brief Turn OFF the selected LED (pin number on port D).
*/
void led_off(uint8_t led_no);


/**
* @brief Crude busy‑wait (software) delay. Not used by the scheduler paths,
* but handy during bring‑up or debugging.
*/
void led_delay(uint32_t count);


#endif // LED_H_
