#include "temp_humi_monitor.h"
DHT20 dht20;

/**
 * @brief Temperature and Humidity Monitoring Task with Semaphore Signaling
 *
 * This task reads temperature and humidity data from the DHT20 sensor
 * and notifies the LED task via semaphore when new data is available.
 *
 * FUNCTIONALITY:
 * 1. Reads temperature and humidity from DHT20 sensor every 5 seconds
 * 2. Updates global variables (glob_temperature, glob_humidity)
 * 3. Signals the LED task via xTempUpdateSemaphore
 * 4. Handles sensor read failures gracefully
 *
 * SEMAPHORE USAGE:
 * - After successfully reading and updating temperature/humidity values
 * - Gives the xTempUpdateSemaphore to notify the LED task
 * - This creates a producer-consumer pattern:
 *   * temp_humi_monitor = Producer (generates temperature data)
 *   * led_blinky = Consumer (consumes temperature data to adjust LED)
 *
 * SYNCHRONIZATION BENEFITS:
 * - Ensures LED behavior changes only when new temperature data is available
 * - Prevents race conditions on shared temperature variable
 * - Creates efficient event-driven communication between tasks
 * - LED task doesn't need to continuously poll temperature value
 */

void temp_humi_monitor(void *pvParameters)
{

    Wire.begin(11, 12);
    Serial.begin(115200);
    dht20.begin();

    Serial.println("Temperature/Humidity Monitor Task Started");
    Serial.println("Sensor: DHT20");
    Serial.println("Update interval: 5 seconds");
    Serial.println("----------------------------------------");

    while (1)
    {
        /* Read sensor data */

        dht20.read();
        // Reading temperature in Celsius
        float temperature = dht20.getTemperature();
        // Reading humidity
        float humidity = dht20.getHumidity();

        // Check if any reads failed and exit early
        if (isnan(temperature) || isnan(humidity))
        {
            Serial.println("TEMP Task: Failed to read from DHT sensor!");
            temperature = humidity = -1;
            // return;
        }
        else
        {
            // Successfully read sensor data

            // // Update global variables for temperature and humidity
            // glob_temperature = temperature;
            // glob_humidity = humidity;
            if (xSemaphoreTake(xMutexSensorData, pdMS_TO_TICKS(100)) == pdTRUE)
            {
                latestSensorData.temperature = temperature;
                latestSensorData.humidity = humidity;
                latestSensorData.timestamp = millis();
                xSemaphoreGive(xMutexSensorData);
            }
            // Print the results
            Serial.println("----------------------------------------");
            Serial.print("TEMP Task: Humidity: ");
            Serial.print(humidity);
            Serial.print("%  Temperature: ");
            Serial.print(temperature);
            Serial.println("°C");

            // CRITICAL: Give semaphores to notify LED and NeoPixel tasks
            // This allows tasks to immediately respond to sensor changes

            // TASK 1: Notify LED task of temperature update
            if (xSemaphoreGive(xTempUpdateSemaphore) == pdTRUE)
            {
                Serial.println("TEMP Task: Temperature semaphore given - LED task notified");
            }
            else
            {
                Serial.println("TEMP Task: Warning - Failed to give temperature semaphore");
            }

            // TASK 2: Notify NeoPixel task of humidity update
            if (xSemaphoreGive(xHumidityUpdateSemaphore) == pdTRUE)
            {
                Serial.println("TEMP Task: Humidity semaphore given - NeoPixel task notified");
            }
            else
            {
                Serial.println("TEMP Task: Warning - Failed to give humidity semaphore");
            }

            // TASK 3: Send sensor data to queue (for LCD and other consumers)
            SensorData_t sensorData;
            sensorData.temperature = temperature;
            sensorData.humidity = humidity;
            sensorData.timestamp = millis();

            if (xQueueSend(xSensorDataQueue, &sensorData, pdMS_TO_TICKS(100)) == pdTRUE)
            {
                Serial.println("TEMP Task: Sensor data sent to queue");
            }
            else
            {
                Serial.println("TEMP Task: Warning - Queue full, data not sent");
            }

            Serial.println("----------------------------------------");
        }

        // Wait 5 seconds before next reading
        vTaskDelay(5000);
    }
}