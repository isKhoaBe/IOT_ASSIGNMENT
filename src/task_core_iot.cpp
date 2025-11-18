
#include "task_core_iot.h"

constexpr uint32_t MAX_MESSAGE_SIZE = 1024U;

WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE);

constexpr char LED_STATE_ATTR[] = "ledState";

volatile int ledMode = 0;
volatile bool ledState = false;

constexpr uint16_t BLINKING_INTERVAL_MS_MIN = 10U;
constexpr uint16_t BLINKING_INTERVAL_MS_MAX = 60000U;
volatile uint16_t blinkingInterval = 1000U;

constexpr int16_t telemetrySendInterval = 10000U;

constexpr std::array<const char *, 2U> SHARED_ATTRIBUTES_LIST = {
    LED_STATE_ATTR,
};

void processSharedAttributes(const Shared_Attribute_Data &data)
{
    for (auto it = data.begin(); it != data.end(); ++it)
    {
        // if (strcmp(it->key().c_str(), BLINKING_INTERVAL_ATTR) == 0)
        // {
        //     const uint16_t new_interval = it->value().as<uint16_t>();
        //     if (new_interval >= BLINKING_INTERVAL_MS_MIN && new_interval <= BLINKING_INTERVAL_MS_MAX)
        //     {
        //         blinkingInterval = new_interval;
        //         Serial.print("Blinking interval is set to: ");
        //         Y
        //             Serial.println(new_interval);
        //     }
        // }
        // if (strcmp(it->key().c_str(), LED_STATE_ATTR) == 0)
        // {
        //     ledState = it->value().as<bool>();
        // digitalWrite(LED_PIN, ledState);
        // Serial.print("LED state is set to: ");
        // Serial.println(ledState);
        // }
    }
}

RPC_Response setLedSwitchValue(const RPC_Data &data)
{
    Serial.println("Received Switch state");
    bool newState = data;
    Serial.print("Switch state change: ");
    Serial.println(newState);
    return RPC_Response("setLedSwitchValue", newState);
}

const std::array<RPC_Callback, 1U> callbacks = {
    RPC_Callback{"setLedSwitchValue", setLedSwitchValue}};

const Shared_Attribute_Callback attributes_callback(&processSharedAttributes, SHARED_ATTRIBUTES_LIST.cbegin(), SHARED_ATTRIBUTES_LIST.cend());
const Attribute_Request_Callback attribute_shared_request_callback(&processSharedAttributes, SHARED_ATTRIBUTES_LIST.cbegin(), SHARED_ATTRIBUTES_LIST.cend());

void CORE_IOT_sendata(String mode, String feed, String data)
{
    if (mode == "attribute")
    {
        tb.sendAttributeData(feed.c_str(), data);
    }
    else if (mode == "telemetry")
    {
        float value = data.toFloat();
        tb.sendTelemetryData(feed.c_str(), value);
    }
    else
    {
        // handle unknown mode
    }
}

void CORE_IOT_reconnect()
{
    const char *ssid = currentSettings.ssid.c_str();
    const char *password = currentSettings.password.c_str();

    // BƯỚC 1: Quản lý kết nối WiFi STA
    if (WiFi.status() != WL_CONNECTED && ssid[0] != '\0')
    {
        Serial.printf("🌐 Đang cố gắng kết nối STA: %s\n", ssid);
        WiFi.mode(WIFI_STA);

        if (password[0] != '\0')
        {
            WiFi.begin(ssid, password);
        }
        else
        {
            WiFi.begin(ssid);
        }

        int count = 0;
        while (WiFi.status() != WL_CONNECTED && count < 20)
        {
            vTaskDelay(pdMS_TO_TICKS(500));
            Serial.print(".");
            count++;
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.printf("\n✅ Kết nối STA thành công! IP: %s\n", WiFi.localIP().toString().c_str());
            isWifiConnected = true;
            xSemaphoreGive(xBinarySemaphoreInternet); // Thông báo kết nối Internet thành công
        }
        else
        {
            Serial.println("\n❌ Kết nối STA thất bại/Timeout.");
            isWifiConnected = false;
            return;
        }

        // BƯỚC 2: Quản lý kết nối ThingsBoard/CoreIOT
        if (WiFi.status() == WL_CONNECTED)
        {
            if (!tb.connected())
            {
                const char *token = currentSettings.token.c_str();
                const char *server = currentSettings.server.c_str();
                int port = currentSettings.port;
                Serial.println("🔑 Đang kết nối ThingsBoard/CoreIOT...");

                if (tb.connect(server, token, port))
                {
                    tb.sendAttributeData("macAddress", WiFi.macAddress().c_str());
                    Serial.println("✅ Kết nối ThingsBoard/CoreIOT thành công!");
                }
                else
                {
                    Serial.println("❌ Kết nối ThingsBoard/CoreIOT thất bại!");
                    return;
                }
            }
        }
        tb.loop();
    }
}

void coreiot_task(void *pvParameters)
{
    loadSettingsFromNVS();
    SettingsCommand newSettings;

    while (1)
    {
        // --- 1. Nhận lệnh Cấu hình (Task 4) ---
        if (xQueueReceive(xQueueSettings, &newSettings, 0) == pdPASS)
        {
            currentSettings = newSettings;
            saveSettingsToNVS(currentSettings);
            WiFi.disconnect(true); // Ngắt kết nối WiFi hiện tại để tái kết nối với cài đặt mới
            Serial.println("⚙️ Đã nhận cấu hình mới. Đang chuẩn bị kết nối lại.");
        }

        // --- 2. Quản lý Kết nối ---
        CORE_IOT_reconnect();

        // --- 3. Publish Telemetry ---
        if (tb.connected())
        {
            // Lấy dữ liệu sensor an toàn (đã sửa Task 3)
            SensorData_t data = getLatestSensorData();

            if (!isnan(data.temperature) && !isnan(data.humidity))
            {

                // Gửi Telemetry (Tận dụng hàm CORE_IOT_sendata)
                // CORE_IOT_sendata sử dụng tb.sendTelemetryData(feed, value);

                // Gửi Nhiệt độ
                CORE_IOT_sendata("telemetry", "temperature", String(data.temperature, 1));
                // Gửi Độ ẩm
                CORE_IOT_sendata("telemetry", "humidity", String(data.humidity, 1));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(telemetrySendInterval)); // Publish theo interval
    }
}

void loadSettingsFromNVS()
{
    preferences.begin("iot_settings", true); // namespace, read-only

    // Tải dữ liệu vào currentSettings
    String loaded_ssid = preferences.getString("ssid", "");
    String loaded_pass = preferences.getString("pass", "");
    String loaded_token = preferences.getString("token", "");
    String loaded_server = preferences.getString("server", "");
    int loaded_port = preferences.getInt("port", 1883); // Giá trị mặc định là 1883

    // Cập nhật currentSettings (Nếu dùng String, chỉ cần gán)
    currentSettings.ssid = loaded_ssid;
    currentSettings.password = loaded_pass;
    currentSettings.token = loaded_token;
    currentSettings.server = loaded_server;
    currentSettings.port = loaded_port;

    preferences.end();
    Serial.printf("⚙️ Đã tải cấu hình: SSID=%s, Token=%s\n", currentSettings.ssid.c_str(), currentSettings.token.c_str());
}

// Hàm lưu cấu hình mới vào NVS
void saveSettingsToNVS(const SettingsCommand &settings)
{
    preferences.begin("iot_settings", false); // namespace, read-write

    preferences.putString("ssid", settings.ssid);
    preferences.putString("pass", settings.password);
    preferences.putString("token", settings.token);
    preferences.putString("server", settings.server);
    preferences.putInt("port", settings.port);

    preferences.end();
    Serial.println("✅ Đã lưu cấu hình mới vào bộ nhớ Flash (NVS).");
}