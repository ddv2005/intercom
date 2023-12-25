#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <stdint.h>
#include <freertos\FreeRTOS.h>
#include <freertos\semphr.h>

#define DEBUG_PORT Serial // Serial debug port
#ifdef DEBUG_PORT
extern SemaphoreHandle_t debugLock;
#define DEBUG_MSG(...) ({xSemaphoreTake(debugLock, portMAX_DELAY); DEBUG_PORT.printf(__VA_ARGS__); xSemaphoreGive(debugLock);})
#define LOCK_DEBUG_PORT() (xSemaphoreTake(debugLock, portMAX_DELAY))
#define UNLOCK_DEBUG_PORT() (xSemaphoreGive(debugLock))
#else
#define DEBUG_MSG(...)
#define LOCK_DEBUG_PORT()
#define UNLOCK_DEBUG_PORT()
#endif

#define xstr(s) ssstr(s)
#define ssstr(s) #s

#define APP_WATCHDOG_SECS_INIT 70
#define APP_WATCHDOG_SECS_WORK 15

#define MAIN_BTN_PULLUP 1
#define HOST_TIMEOUT  15000
#define BTN_GUARD_TIME    100
#define INP_GUARD_TIME    100
#define MAX_PACKET_SIZE   32

#define PIN_LED      2
#define PIN_BTN_LED  18

#define PIN_RELAY    32
#define PIN_BTN      23
#define PIN_INP1     26
#define PIN_INP2     13
#define PIN_BTN2_LED   19
#define PIN_UART_TX    17
#define PIN_UART_RX    16

#define HAS_ULTRASONIC
#define PIN_ULTRASONIC 33

#endif