#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

extern float glob_temperature;
extern float glob_humidity;

extern String WIFI_SSID;
extern String WIFI_PASS;
extern String CORE_IOT_TOKEN;
extern String CORE_IOT_SERVER;
extern String CORE_IOT_PORT;

extern boolean isWifiConnected;
extern SemaphoreHandle_t xBinarySemaphoreInternet;

// TASK 1: Semaphore for temperature-LED synchronization
extern SemaphoreHandle_t xTempUpdateSemaphore;

// TASK 2: Semaphore for humidity-NeoPixel synchronization
extern SemaphoreHandle_t xHumidityUpdateSemaphore;

// TASK 3: Structure for sensor data (replaces global variables)
typedef struct
{
    float temperature;
    float humidity;
    unsigned long timestamp;
} SensorData_t;

// TASK 3: Queue for sensor data communication (replaces globals)
extern QueueHandle_t xSensorDataQueue;

// TASK 3: Semaphore for LCD display state control
extern SemaphoreHandle_t xLCDStateSemaphore;

// TASK 3: Display states
typedef enum
{
    DISPLAY_STATE_NORMAL,
    DISPLAY_STATE_WARNING,
    DISPLAY_STATE_CRITICAL
} DisplayState_t;

extern DisplayState_t currentDisplayState;
// Cấu trúc chứa dữ liệu Sensor mới nhất
extern SensorData_t latestSensorData;
// Mutex Handle để bảo vệ latestSensorData
extern SemaphoreHandle_t xMutexSensorData;

#endif