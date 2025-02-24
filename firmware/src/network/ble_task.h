#pragma once

#if SK_BLE

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Preferences.h>

#include "../logger.h"
#include "../task.h"

#include "../proto_gen/smartknob.pb.h"


enum class MessageType {
    READ,
    WRITE,
    NOTIFY,
    INDICATE,
};

struct Message {
    MessageType message_type;
    union MessageData {
        uint8_t unused;
        PB_SmartKnobConfig config;
    };
    MessageData data;
};

class BLETask : public Task<BLETask> {
    friend class Task<BLETask>; // Allow base Task to invoke protected run()

    public:
        BLETask(const uint8_t task_core);
        ~BLETask();
        // void setConfig(const PB_SmartKnobConfig& config);
        // void playHaptic(bool press);
        // void runCalibration();

        void addListener(QueueHandle_t queue);
        void setLogger(Logger* logger);
        friend class KnobServerCallbacks;

    protected:
        void run();

    private:
        QueueHandle_t queue_;
        Logger* logger_;
        std::vector<QueueHandle_t> listeners_;
        char buf_[72];

        // BLE Setup
        BLEServer* pServer = NULL;
        BLECharacteristic* pCharacteristic = NULL;
        // bool BLE_CONNECTED;
        bool oldDeviceConnected = false;
        uint32_t test_value = 0;

        void publish(const PB_SmartKnobState& state);
        // void calibrate();
        // void checkSensorError();
        void log(const char* msg);
};

#endif