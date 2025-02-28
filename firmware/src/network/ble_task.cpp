#if SK_BLE

#include "ble_task.h"

bool deviceConnected = false;

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
        std::string rxValue = pCharacteristic->getValue();
        Serial.print("value received = ");
        Serial.println(rxValue.c_str());
        log_i("data is received"); 
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
        if (deviceConnected) {
                        
            if(button_state_old != button_state_){
                button_state_old = button_state_;
                sendNotify(1, button_state_old);
            }

            if(press_value_unit_old != press_value_unit_){
                press_value_unit_old = press_value_unit_;
                sendNotify(2, press_value_unit_old);
            }

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

            if (lux_value_old != lux_value_){
                lux_value_old = lux_value_;
                sendNotify(6, (uint32_t)&lux_value_old);
            }
            
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
