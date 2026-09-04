#pragma once

#include <AceButton.h>
#include <Arduino.h>
#include <Preferences.h>

#include "display_task.h"
#include "logger.h"
#include "motor_task.h"
#include "network/ble_task.h"
#include "network/tcp_stream.h"
#include "network/wifi_config.h"
#include "serial/serial_protocol_plaintext.h"
#include "serial/serial_protocol_protobuf.h"
#include "serial/uart_stream.h"
#include "task.h"

class InterfaceTask : public Task<InterfaceTask>, public Logger {
    friend class Task<InterfaceTask>; // Allow base Task to invoke protected run()

    public:
        InterfaceTask(const uint8_t task_core, MotorTask& motor_task, BLETask& ble_task, DisplayTask* display_task);
        virtual ~InterfaceTask() {};

        void log(const char* msg) override;

    protected:
        void run();

    private:
        UartStream stream_;
        MotorTask& motor_task_;
        BLETask& ble_task_;
        DisplayTask* display_task_;
        char buf_[64];

        int current_config_ = 0;

        QueueHandle_t log_queue_;
        QueueHandle_t knob_state_queue_;
        SerialProtocolPlaintext plaintext_protocol_;
        SerialProtocolProtobuf proto_protocol_;

        TcpStream tcp_stream_;
        SerialProtocolPlaintext tcp_protocol_;
        bool ota_requested_ = false;
        bool ota_in_progress_ = false;

        // Press threshold on the normalized [0, 1] strain range; releases at a third of it
        Preferences preferences_;
        float press_actuation_ = 0.75;

        // Compact log of the last OTA-mode attempt; persisted to NVS so it can be
        // reported over BLE after the reboot back to normal mode
        char ota_diag_[192] = {0};
        void otaDiagAppend(const char* fmt, ...);

        void changeConfig(bool next);
        void updateHardware();
        void runOtaMode();
};
