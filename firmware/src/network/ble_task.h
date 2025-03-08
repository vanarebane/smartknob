#pragma once

#if SK_BLE

#include <cstring> // Include for std::strncpy, std::strtok, and std::strncmp

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Preferences.h>

#include "../logger.h"
#include "../proto_gen/smartknob.pb.h"
#include "../task.h"


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

        void updateScale(int32_t press_value_unit);
        void updateScale(bool press_value_state);
        void updateButton(bool button_state);
        void updateLux(float new_lux_value);
        bool hasInputFromBT();
        bool hasNewMotorProfile();
        bool hasNewMotorConfig();
        PB_SmartKnobConfig getMotorProfile();
        MotorConfig getMotorConfig();
        

        QueueHandle_t getKnobStateQueue();
        void addListener(QueueHandle_t queue);

        void setLogger(Logger* logger);
        friend class KnobServerCallbacks;

    protected:
        void run();

    private:
        QueueHandle_t queue_;
        Logger* logger_;
        std::vector<QueueHandle_t> listeners_;

        int ble_delay = 7;

        // BLE Setup
        BLEServer* pServer = NULL;
        BLECharacteristic* pTxCharacteristic;
        BLECharacteristic* pRxCharacteristic;
        // bool BLE_CONNECTED;
        bool oldDeviceConnected = false;

        int32_t response = 0;
        uint32_t press_value_unit_;
        uint32_t press_value_unit_old;
        bool press_state_;
        bool press_state_old;
        bool button_state_;
        bool button_state_old;
        uint32_t current_position;
        uint32_t num_positions;
        float sub_positions;
        float lux_value_;
        float lux_value_old;
        float subpos;

        PB_SmartKnobConfig new_motor_profile;
        MotorConfig new_motor_config;
        bool hasNewMotorConfig_ = false;
        bool hasNewMotorProfile_ = false;

        QueueHandle_t knob_state_queue_;

        PB_SmartKnobState state_;
        SemaphoreHandle_t mutex_;

        void sendNotify(int, bool);
        void sendNotify(int, uint32_t);
        void sendNotify(int, float&);
        bool parseInputProfile(const char* input, PB_SmartKnobConfig& data);
        bool parseInputConfig(const char* input, MotorConfig& data);
        

        void publish(const PB_SmartKnobState& state);
        // void calibrate();
        // void checkSensorError();
        void log(const char* msg);
};

#endif