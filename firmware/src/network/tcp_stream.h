#pragma once

#include <Arduino.h>
#include <WiFi.h>

/**
 * Arduino Stream backed by a TCP server socket, so the existing serial
 * protocols can run over WiFi while in OTA mode. Single client; a new
 * connection replaces the previous one. Writes with no client are dropped.
 */
class TcpStream : public Stream {
    public:
        TcpStream(uint16_t port) : server_(port) {}

        void begin();
        void loop();
        bool hasClient();

        // Stream methods
        int available() override;
        int read() override;
        int peek() override;
        void flush() override;

        // Print methods
        size_t write(uint8_t b) override;
        size_t write(const uint8_t *buffer, size_t size) override;

    private:
        WiFiServer server_;
        WiFiClient client_;
};
