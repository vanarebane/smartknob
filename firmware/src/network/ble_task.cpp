#if SK_BLE

#include "ble_task.h"

bool deviceConnected = false;

bool ble_quiet_please = false;
bool hasNewMotorConfig_ = false;
bool newMotorConfigProccessed = true;
char* newMotorConfig_;

class KnobServerCallbacks: public BLEServerCallbacks {

    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};

class KnobCharacteristicCallBack : public BLECharacteristicCallbacks{
public:
    //This method not called
    void onWrite(BLECharacteristic *pCharacteristic) override{
        // std::string xcc = pCharacteristic->getValue();
        // // uint8_t* rxValue = pCharacteristic->getData();
        // Serial.print("value received = ");
        // Serial.println((char*)&xcc);
        ble_quiet_please = true;

        char* rxValue = reinterpret_cast<char*>(pCharacteristic->getData());
        
        // char buf_[256];
        // snprintf(buf_, sizeof(buf_), "value received from BT = %s", val);
        // log(val);

        log_i("data is received");

        hasNewMotorConfig_ = true;
        newMotorConfigProccessed = false;
        newMotorConfig_ = rxValue;
        delay(100);
        ble_quiet_please = false;
    }
};

BLETask::BLETask(const uint8_t task_core) : Task("BLE", 2700, 1, task_core) {
    queue_ = xQueueCreate(5, sizeof(Message));
    assert(queue_ != NULL);
    
    knob_state_queue_ = xQueueCreate(1, sizeof(PB_SmartKnobState));
    assert(knob_state_queue_ != NULL);

    mutex_ = xSemaphoreCreateMutex();
    assert(mutex_ != NULL);
}

BLETask::~BLETask() {
    vQueueDelete(knob_state_queue_);
    vSemaphoreDelete(mutex_);
}

void BLETask::run() {
     
    // See the following for generating UUIDs:
    // https://www.uuidgenerator.net/

    #define SERVICE_UUID        "0000340f-0000-1000-8000-00805f9b34fb"
    #define CHARACTERISTIC_UUID   "602f75a0-697d-4690-8dd5-fa52781446d1"

    // Create the BLE Device
    BLEDevice::init("SmartKnob_0123");

    // Create the BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new KnobServerCallbacks());
    

    // Create the BLE Service
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml

    // Create a BLE Characteristic
    pCharacteristic = pService->createCharacteristic(
            CHARACTERISTIC_UUID,
            BLECharacteristic::PROPERTY_READ   |
            BLECharacteristic::PROPERTY_WRITE  |
            BLECharacteristic::PROPERTY_NOTIFY |
            BLECharacteristic::PROPERTY_INDICATE
        );
        
    pCharacteristic->setCallbacks(new KnobCharacteristicCallBack());
    // Create a BLE Descriptor
    pCharacteristic->addDescriptor(new BLE2902());


    // Start the service
    pService->start();

    // Start advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(false);
    pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
    BLEDevice::startAdvertising();

    PB_SmartKnobState state;

    while (1) {
        
        // disconnecting
        if (!deviceConnected && oldDeviceConnected) {
            delay(500); // give the bluetooth stack the chance to get things ready
            pServer->startAdvertising(); // restart advertising
            log("BLE start advertising");
            oldDeviceConnected = deviceConnected;
        }
        // connecting
        if (deviceConnected && !oldDeviceConnected) {
            // do stuff here on connecting
            oldDeviceConnected = deviceConnected;
            //delay(500);
            //pCharacteristic->notify(); // DOES NOT WORK
        }

        // Old method here, skip everything if no update from knob
        if (xQueueReceive(knob_state_queue_, &state, portMAX_DELAY) == pdFALSE) {
            continue;
        }

        // notify changed value
        if (deviceConnected && !ble_quiet_please) {
                        
            if(button_state_old != button_state_){
                button_state_old = button_state_;
                sendNotify(1, button_state_old);
            }

            // if(press_value_unit_old != press_value_unit_){
            //     press_value_unit_old = press_value_unit_;
            //     sendNotify(2, press_value_unit_old);
            // }

            if (press_state_old != press_state_){
                press_state_old = press_state_;
                sendNotify(3, press_state_old);
            }

            if (current_position != state.current_position){
                current_position = state.current_position;
                sendNotify(4, current_position);
            }

            if (num_positions != state.config.num_positions){
                num_positions = state.config.num_positions;
                sendNotify(5, num_positions);
            }

            // if (lux_value_old != lux_value_){
            //     lux_value_old = lux_value_;
            //     sendNotify(6, (uint32_t)&lux_value_old);
            // }
            
        }

        // // Check queue for pending requests from other tasks
        // Message message;
        // if (xQueueReceive(queue_, &message, 0) == pdTRUE) {
        //     switch (message.message_type) {
        //         case MessageType::READ:{
        //             break;
        //         }
        //         case MessageType::WRITE: {
        //             break;
        //         }
        //         case MessageType::NOTIFY: {
        //             break;
        //         }
        //         case MessageType::INDICATE: {
        //             break;
        //         }
        //     }
        // }

    }

}

void BLETask::sendNotify(int key, bool data){
	uint8_t temp[2];
	temp[0] = key;
    if(data) temp[1] = 1;
    else temp[1] = 0;
    pCharacteristic->setValue((uint8_t*)&temp, 2);
    pCharacteristic->notify();
    delay(3); // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
}
void BLETask::sendNotify(int key, uint32_t data32){
	uint32_t temp[8];
	temp[0] = key;
	temp[1] = data32;
	temp[2] = data32 >> 8;
	temp[3] = data32 >> 16;
	temp[4] = data32 >> 24;
	temp[5] = data32 >> 32;
	temp[6] = data32 >> 40;
	temp[7] = data32 >> 48;
    pCharacteristic->setValue((uint8_t*)&temp,8);
    pCharacteristic->notify();
    delay(3); // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
}

void BLETask::updateScale(int32_t new_press_value_unit){
    press_value_unit_ = new_press_value_unit;
}

void BLETask::updateScale(bool new_press_state){
    press_state_ = new_press_state;
}

void BLETask::updateLux(float new_lux_value){
    lux_value_ = new_lux_value;
}

void BLETask::updateButton(bool new_button_state){
    button_state_ = new_button_state;
}


bool BLETask::hasNewMotorConfig(){

    if(hasNewMotorConfig_){
        if(newMotorConfigProccessed){
            return true;
        }
        else{            
            log("Received from BT: ");
            log(newMotorConfig_);

            // already set to true
            newMotorConfigProccessed = true;
            // Create the struct instance
            PB_SmartKnobConfig config;

            // Parse the input and populate the struct
            if (parseInput(newMotorConfig_, config)) {
                new_motor_config = config;
                return true;
            } else {
                log("Invalid input");
                hasNewMotorConfig_ = false;
                return false;
            }
        }
    }

    else return false;

}

PB_SmartKnobConfig BLETask::getMotorConfig(){
    
    hasNewMotorConfig_ = false;
    return new_motor_config;

    // PB_SmartKnobConfig config = {
    //         32,
    //         0,
    //         8.225806452 * PI / 180,
    //         0.2,
    //         1,
    //         1.1,
    //         "Fooo",
    // };
    // return config;
}

// Function to parse the input string and populate the struct
bool BLETask::parseInput(const char* input, PB_SmartKnobConfig& data) {
    // // Check for "SP+" prefix
    if (std::strncmp(input, "SP+", 3) != 0) {
        return false;
    }

    // Extract the relevant part after "SP+"
    const char* dataString = input + 3;

    // // Split the string by commas
    std::vector<const char*> tokens;
    char* mutableDataString = new char[std::strlen(dataString) + 1];
    std::strcpy(mutableDataString, dataString);

    char* token = std::strtok(mutableDataString, ",");
    while (token != nullptr) {
        tokens.push_back(token);
        token = std::strtok(nullptr, ",");
    }

    // Check if we have the expected number of values
    if (tokens.size() != 7) {
        delete[] mutableDataString;
        return false;
    }

    // int32_t num_positions;
    // int32_t position;
    // float position_width_radians; // remember  * PI / 180
    // float detent_strength_unit;
    // float endstop_strength_unit;
    // float snap_point;
    // char text[51];
    
    // Convert and assign the values to the struct
    try {
        data.num_positions = std::atoi(tokens[0]);
        data.position = std::atoi(tokens[1]);
        data.position_width_radians = std::atof(tokens[2]) * PI / 180;
        data.detent_strength_unit = std::atof(tokens[3]);
        data.endstop_strength_unit = std::atof(tokens[4]);
        data.snap_point = std::atof(tokens[5]);
        std::strncpy(data.text, tokens[6], sizeof(data.text) - 1);
        data.text[sizeof(data.text) - 1] = '\0'; // Ensure null termination
    } catch (...) {
        delete[] mutableDataString;
        return false;
    }

    delete[] mutableDataString;
    return true;
}

void BLETask::addListener(QueueHandle_t queue) {
    listeners_.push_back(queue);
}

void BLETask::publish(const PB_SmartKnobState& state) {
    for (auto listener : listeners_) {
        xQueueOverwrite(listener, &state);
    }
}

QueueHandle_t BLETask::getKnobStateQueue() {
    return knob_state_queue_;
}


void BLETask::setLogger(Logger* logger) {
    logger_ = logger;
}

void BLETask::log(const char* msg) {
    if (logger_ != nullptr) {
        logger_->log(msg);
    }
}

#else

class BLETask {};

#endif
