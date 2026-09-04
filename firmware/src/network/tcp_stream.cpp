#include "tcp_stream.h"

void TcpStream::begin() {
    server_.begin();
    server_.setNoDelay(true);
}

void TcpStream::loop() {
    if (server_.hasClient()) {
        WiFiClient new_client = server_.available();
        if (client_ && client_.connected()) {
            client_.stop();
        }
        client_ = new_client;
    }
}

bool TcpStream::hasClient() {
    return client_ && client_.connected();
}

int TcpStream::available() {
    return hasClient() ? client_.available() : 0;
}

int TcpStream::read() {
    return hasClient() ? client_.read() : -1;
}

int TcpStream::peek() {
    return hasClient() ? client_.peek() : -1;
}

void TcpStream::flush() {
    if (hasClient()) {
        client_.flush();
    }
}

size_t TcpStream::write(uint8_t b) {
    return hasClient() ? client_.write(b) : 0;
}

size_t TcpStream::write(const uint8_t *buffer, size_t size) {
    return hasClient() ? client_.write(buffer, size) : 0;
}
