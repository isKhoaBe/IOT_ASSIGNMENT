#include "task_webserver.h"

// Giả định Pin cho LED1 và LED2 (Bạn phải thay thế bằng GPIO thực tế)
#define LED1_GPIO 2
#define LED2_GPIO 4

// Mảng theo dõi trạng thái hiện tại (Đã loại bỏ biến global T/H theo yêu cầu đề bài, nhưng biến này là cục bộ trong Task)
bool currentLedState[3] = {false, false, false}; // Index 1: LED1, Index 2: LED2

void Relay_Control_Task(void *pvParameters)
{
    // Cài đặt Pin Mode
    pinMode(LED1_GPIO, OUTPUT);
    pinMode(LED2_GPIO, OUTPUT);
    // Đảm bảo trạng thái ban đầu là OFF
    digitalWrite(LED1_GPIO, LOW);
    digitalWrite(LED2_GPIO, LOW);

    RelayControlCommand cmd;

    while (1)
    {
        // Chờ nhận lệnh từ Queue (Block Task cho đến khi có lệnh)
        if (xQueueReceive(xQueueRelayControl, &cmd, portMAX_DELAY) == pdPASS)
        {

            int pin = cmd.gpioPin;
            bool state = cmd.newState;

            // Xử lý lệnh điều khiển
            if (pin == LED1_GPIO || pin == LED2_GPIO)
            {

                // === 1. Điều khiển Vật lý ===
                digitalWrite(pin, state ? HIGH : LOW);

                // === 2. Cập nhật trạng thái cục bộ ===
                if (pin == LED1_GPIO)
                    currentLedState[1] = state;
                else if (pin == LED2_GPIO)
                    currentLedState[2] = state;

                // === 3. Gửi Phản hồi Trạng thái qua WebSocket ===

                // Chuẩn bị JSON phản hồi: {"page":"device_update", "value":{"gpio":2, "status":"ON"}}
                StaticJsonDocument<256> jsonDoc;
                jsonDoc["page"] = "device_update";
                jsonDoc["value"]["gpio"] = pin;
                jsonDoc["value"]["status"] = state ? "ON" : "OFF";

                String jsonString;
                serializeJson(jsonDoc, jsonString);

                // Gửi JSON ngược lại cho tất cả client
                Webserver_sendata(jsonString);

                Serial.printf("✅ RELAY GPIO %d set to %s. Response sent via WS.\n", pin, state ? "ON" : "OFF");
            }
            // Nếu bạn có thêm thiết bị GPIO khác, bạn có thể thêm logic kiểm tra pin ở đây
        }
        // Không cần vTaskDelay vì Task đã bị block bởi xQueueReceive.
    }
}