#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>

class Websocket {
public:
    Websocket(
        const char* wifiSsid,
        const char* wifiPassword,
        const char* serverIp,
        uint16_t serverPort,
        const char* serverPath,
        const char* clientId
    )
        : wifiSsid_(wifiSsid), wifiPassword_(wifiPassword),
          serverIp_(serverIp), serverPort_(serverPort),
          serverPath_(serverPath), clientId_(clientId) {
        active() = this;
    }

    void begin() {
        WiFi.begin(wifiSsid_, wifiPassword_);
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
        }

        Serial.println();
        Serial.println("Wi-Fi connected");
        client_.begin(serverIp_, serverPort_, serverPath_);
        client_.onEvent(eventCallback);
        client_.setReconnectInterval(5000);
        client_.enableHeartbeat(15000, 3000, 2);
    }

    void loop() { client_.loop(); }

    bool isAuthenticated() const { return authenticated_; }

    bool sendText(const String& message) {
        if (!authenticated_) return false;
        String copy = message;
        return client_.sendTXT(copy);
    }

private:
    static Websocket*& active() {
        static Websocket* instance = nullptr;
        return instance;
    }

    static void eventCallback(WStype_t type, uint8_t* payload, size_t length) {
        if (active() != nullptr) active()->handleEvent(type, payload, length);
    }

    void handleEvent(WStype_t type, uint8_t* payload, size_t length) {
        if (type == WStype_CONNECTED) {
            Serial.println("Connected to WebSocket server");
            client_.sendTXT(clientId_);
            return;
        }

        if (type == WStype_DISCONNECTED) {
            authenticated_ = false;
            Serial.println("Disconnected");
            return;
        }

        if (type == WStype_ERROR) {
            Serial.println("WebSocket error");
            return;
        }

        if (type != WStype_TEXT) return;

        String message;
        for (size_t i = 0; i < length; i++) {
            message += static_cast<char>(payload[i]);
        }

        if (message.indexOf("\"authenticated\"") >= 0 &&
            message.indexOf("\"ok\"") >= 0) {
            authenticated_ = true;
            Serial.println("Client authenticated");
        }

        if (message.indexOf("\"error\"") >= 0) {
            authenticated_ = false;
            Serial.println("Authentication failed");
        }
    }

    const char* wifiSsid_;
    const char* wifiPassword_;
    const char* serverIp_;
    uint16_t serverPort_;
    const char* serverPath_;
    const char* clientId_;
    WebSocketsClient client_;
    bool authenticated_ = false;
};
