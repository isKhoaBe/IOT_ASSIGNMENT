#include "task_webserver.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

typedef struct
{
    int gpioPin;
    bool newState;
} RelayControlCommand;

typedef struct
{
    String ssid;
    String password;
    String token;
    String server;
    int port;
} SettingsCommand;

extern QueueHandle_t xQueueRelayControl; // LED/Relay control queue
extern QueueHandle_t xQueueSettings;     // Wifi/Server settings queue

bool webserver_isrunning = false;

void Webserver_sendata(String data)
{
    if (ws.count() > 0)
    {
        ws.textAll(data); // Gửi đến tất cả client đang kết nối
        Serial.println("📤 Đã gửi dữ liệu qua WebSocket: " + data);
    }
    else
    {
        Serial.println("⚠️ Không có client WebSocket nào đang kết nối!");
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
    }
    else if (type == WS_EVT_DATA)
    {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;

        if (info->opcode == WS_TEXT)
        {
            String message;
            message += String((char *)data).substring(0, len);
            // parseJson(message, true);
            handleWebSocketMessage(message);
        }
    }
}

void connnectWSV()
{
    ws.onEvent(onEvent);
    server.addHandler(&ws);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/index.html", "text/html"); });
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/script.js", "application/javascript"); });
    server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/styles.css", "text/css"); });
    server.begin();
    ElegantOTA.begin(&server);
    webserver_isrunning = true;
}

void Webserver_stop()
{
    ws.closeAll();
    server.end();
    webserver_isrunning = false;
}

void Webserver_reconnect()
{
    if (!webserver_isrunning)
    {
        connnectWSV();
    }
    ElegantOTA.loop();
}

void handleWebSocketMessage(String message)
{
    Serial.println("📥 Đã nhận dữ liệu qua WebSocket: " + message);

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, message);

    if (error)
    {
        Serial.print("⚠️ Lỗi phân tích JSON: ");
        Serial.println(error.f_str());
        return;
    }

    String page = doc["page"].as<String>();

    // ========== Xử lý lệnh điều khiển relay/LED ==========
    if (page == "device")
    {
        String status = doc["value"]["status"].as<String>();
        int gpio = doc["value"]["gpio"].as<int>();

        Serial.printf("Received Relay Command: GPIO %d, Status: %s\n", gpio, status.c_str());

        RelayControlCommand cmd;
        cmd.gpioPin = gpio;
        cmd.newState = (status == "ON") ? true : false;

        if (xQueueRelayControl != NULL)
        {
            if (xQueueSend(xQueueRelayControl, (void *)&cmd, (TickType_t)0) == pdPASS)
                ;
            Serial.println("✅ Đã gửi lệnh điều khiển relay/LED vào queue.");
        }
        else
        {
            Serial.println("⚠️ Queue điều khiển relay/LED không khả dụng!");
        }
    }

    // ========== Xử lý lệnh cập nhật cài đặt ==========
    else if (page == "settings")
    {
        SettingsCommand settings;
        settings.ssid = doc["value"]["ssid"].as<String>();
        settings.password = doc["value"]["password"].as<String>();
        settings.token = doc["value"]["token"].as<String>();
        settings.server = doc["value"]["server"].as<String>();
        settings.port = doc["value"]["port"].as<int>();

        Serial.println("Received Settings Update:");
        Serial.printf("SSID: %s, Password: %s, Token: %s, Server: %s, Port: %d\n",
                      settings.ssid.c_str(), settings.password.c_str(),
                      settings.token.c_str(), settings.server.c_str(),
                      settings.port);

        if (xQueueSettings != NULL)
        {
            BaseType_t result = xQueueSend(xQueueSettings, (void *)&settings, (TickType_t)0);
            if (result == pdPASS)
            {
                Serial.println("✅ Đã gửi lệnh cập nhật cài đặt vào queue.");
            }
            else
            {
                Serial.println("⚠️ Không thể gửi lệnh cập nhật cài đặt vào queue!");
            }
        }
    }
}

void Webserver_RTOS_Task(void *pvParameters)
{
    connnectWSV();

    while (1)
    {
        if (check_info_File(1))
        {
            if (!Wifi_reconnect())
            {
                Webserver_stop();
            }
        }
        Webserver_reconnect();

        vTaskDelay(pdMS_TO_TICKS(50)); // Delay nhỏ để tránh watchdog reset
    }
}