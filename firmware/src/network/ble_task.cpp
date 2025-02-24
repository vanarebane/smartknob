#if SK_BLE

#include "ble_task.h"

BLETask::BLETask(const uint8_t task_core) : Task("BLE", 2500, 1, task_core) {
    queue_ = xQueueCreate(5, sizeof(Message));
    assert(queue_ != NULL);
}

BLETask::~BLETask() {}

bool deviceConnected = false;

class KnobServerCallbacks: public BLEServerCallbacks {

    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};

void BLETask::run() {
     
    // See the following for generating UUIDs:
    // https://www.uuidgenerator.net/

    #define SERVICE_UUID        "0000340f-0000-1000-8000-00805f9b34fb"
    #define CHARACTERISTIC_UUID "000042ff-0000-1000-8000-00805f9b34fb"

    // Create the BLE Device
    BLEDevice::init("SmartKnob_0123");

    // Create the BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new KnobServerCallbacks());

    // Create the BLE Service
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Create a BLE Characteristic
    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_READ   |
                        BLECharacteristic::PROPERTY_WRITE  |
                        BLECharacteristic::PROPERTY_NOTIFY |
                        BLECharacteristic::PROPERTY_INDICATE
                    );

    // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
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

    while (1) {
        
        // Check queue for pending requests from other tasks
        Message message;
        if (xQueueReceive(queue_, &message, 0) == pdTRUE) {
            switch (message.message_type) {
                case MessageType::READ:{
                    break;
                }
                case MessageType::WRITE: {
                    break;
                }
                case MessageType::NOTIFY: {
                    break;
                }
                case MessageType::INDICATE: {
                    break;
                }
            }
        }

        // notify changed value
        if (deviceConnected) {
            pCharacteristic->setValue((uint8_t*)&test_value, 4);
            pCharacteristic->notify();
            test_value++;
            delay(1000);
            //delay(3); // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
        }
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
        }

    }

}

void BLETask::addListener(QueueHandle_t queue) {
    listeners_.push_back(queue);
}

void BLETask::publish(const PB_SmartKnobState& state) {
    for (auto listener : listeners_) {
        xQueueOverwrite(listener, &state);
    }
}

void BLETask::setLogger(Logger* logger) {
    logger_ = logger;
}

void BLETask::log(const char* msg) {
    if (logger_ != nullptr) {
        logger_->log(msg);
    }
}


#endif
