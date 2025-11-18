#include "global.h"
float glob_temperature = 0;
float glob_humidity = 0;

String WIFI_SSID;
String WIFI_PASS;
String CORE_IOT_TOKEN;
String CORE_IOT_SERVER;
String CORE_IOT_PORT;

String ssid = "ESP32-YOUR NETWORK HERE!!!";
String password = "12345678";
String wifi_ssid = "abcde";
String wifi_password = "123456789";
boolean isWifiConnected = false;
SemaphoreHandle_t xBinarySemaphoreInternet = xSemaphoreCreateBinary();

// TASK 1: Semaphore for temperature-LED synchronization
// This semaphore will be used to notify the LED task when temperature data is updated
SemaphoreHandle_t xTempUpdateSemaphore = xSemaphoreCreateBinary();

// TASK 2: Semaphore for humidity-NeoPixel synchronization
// This semaphore notifies the NeoPixel task when humidity data is updated
SemaphoreHandle_t xHumidityUpdateSemaphore = xSemaphoreCreateBinary();

// TASK 3: Queue for sensor data communication (replaces global variables)
// Queue can hold 5 sensor readings to prevent data loss
QueueHandle_t xSensorDataQueue = xQueueCreate(5, sizeof(SensorData_t));

// TASK 3: Semaphore for LCD display state control
// Controls access to display state changes
SemaphoreHandle_t xLCDStateSemaphore = xSemaphoreCreateMutex();

// TASK 3: Current display state
DisplayState_t currentDisplayState = DISPLAY_STATE_NORMAL;

SensorData_t getLatestSensorData()
{
    SensorData_t data;
    if (xSemaphoreTake(xMutexSensorData, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        data = latestSensorData;
        xSemaphoreGive(xMutexSensorData);
    }
    else
    {
        // Trả về dữ liệu trống/lỗi nếu không thể lấy Mutex
        data.temperature = -999;
        data.humidity = -999;
    }
    return data;
}