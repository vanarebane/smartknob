#if SK_LEDS
#include <FastLED.h>
#endif

#if SK_STRAIN
#include <HX711.h>
#endif

#if SK_ALS
#include <Adafruit_VEML7700.h>
#endif

#include <ArduinoOTA.h>
#include <WiFi.h>
#include <esp_wifi.h>

#if SK_BLE
#include <BLEDevice.h>
#endif

#include "interface_task.h"
#include "util.h"

#define COUNT_OF(A) (sizeof(A) / sizeof(A[0]))

#if SK_LEDS
CRGB leds[NUM_LEDS];
#endif

#if SK_STRAIN
HX711 scale;
#endif

#if SK_ALS
Adafruit_VEML7700 veml = Adafruit_VEML7700();
#endif

static PB_SmartKnobConfig configs[] = {
    // int32_t num_positions;
    // int32_t position;
    // float position_width_radians;
    // float detent_strength_unit;
    // float endstop_strength_unit;
    // float snap_point;
    // char text[51];

    {
        0,
        0,
        10 * PI / 180,
        0,
        1,
        1.1,
        "Unbounded\nNo detents",
    },
    {
        11,
        0,
        10 * PI / 180,
        0,
        1,
        1.1,
        "Bounded 0-10\nNo detents",
    },
    {
        73,
        0,
        10 * PI / 180,
        0,
        1,
        1.1,
        "Multi-rev\nNo detents",
    },
    {
        2,
        0,
        60 * PI / 180,
        1,
        1,
        0.55, // Note the snap point is slightly past the midpoint (0.5); compare to normal detents which use a snap point *past* the next value (i.e. > 1)
        "On/off\nStrong detent",
    },
    {
        1,
        0,
        60 * PI / 180,
        0.01,
        0.6,
        1.1,
        "Return-to-center",
    },
    {
        256,
        127,
        1 * PI / 180,
        0,
        1,
        1.1,
        "Fine values\nNo detents",
    },
    {
        256,
        127,
        1 * PI / 180,
        1,
        1,
        1.1,
        "Fine values\nWith detents",
    },
    {
        32,
        0,
        8.225806452 * PI / 180,
        2,
        1,
        1.1,
        "Coarse values\nStrong detents",
    },
    {
        32,
        0,
        8.225806452 * PI / 180,
        0.2,
        1,
        1.1,
        "Coarse values\nWeak detents",
    },
};

InterfaceTask::InterfaceTask(const uint8_t task_core, MotorTask& motor_task, BLETask& ble_task, DisplayTask* display_task) : 
        Task("Interface", 10240, 1, task_core), // Generous stack: OTA mode runs WiFi + ArduinoOTA + mDNS in this task
        stream_(),
        motor_task_(motor_task),
        ble_task_(ble_task),
        display_task_(display_task),
        plaintext_protocol_(stream_, motor_task_),
        proto_protocol_(stream_, motor_task_),
        tcp_stream_(OTA_CONSOLE_PORT),
        tcp_protocol_(tcp_stream_, motor_task_) {
    #if SK_DISPLAY
        assert(display_task != nullptr);
    #endif

    #if PIN_BUTTON_NEXT
        pinMode(PIN_BUTTON_NEXT, INPUT_PULLUP);
    #endif

    log_queue_ = xQueueCreate(10, sizeof(std::string *));
    assert(log_queue_ != NULL);

    knob_state_queue_ = xQueueCreate(1, sizeof(PB_SmartKnobState));
    assert(knob_state_queue_ != NULL);
}

void InterfaceTask::run() {
    stream_.begin();
    
    #if SK_LEDS
        FastLED.addLeds<SK6812, PIN_LED_DATA, GRB>(leds, NUM_LEDS);
    #endif

    #if SK_ALS && PIN_SDA >= 0 && PIN_SCL >= 0
        Wire.begin(PIN_SDA, PIN_SCL);
        Wire.setClock(400000);
    #endif
    
    #if SK_STRAIN
        scale.begin(PIN_STRAIN_DO, PIN_STRAIN_SCK);
    #endif

    #if SK_ALS
        if (veml.begin()) {
            veml.setGain(VEML7700_GAIN_2);
            veml.setIntegrationTime(VEML7700_IT_400MS);
        } else {
            log("ALS sensor not found!");
        }
    #endif

    motor_task_.setConfig(configs[0]);
    motor_task_.addListener(knob_state_queue_);

    preferences_.begin("knob", false);
    press_actuation_ = preferences_.getFloat("actuation", press_actuation_);
    snprintf(buf_, sizeof(buf_), "Press actuation point: %.2f", press_actuation_);
    log(buf_);
    #if SK_BLE
        ble_task_.updateActuation(press_actuation_);

        // Report the previous OTA-mode attempt over BLE (survives the reboot via NVS)
        String last_diag = preferences_.getString("ota_diag", "");
        if (last_diag.length() > 0) {
            ble_task_.setDiagText(last_diag.c_str());
            snprintf(buf_, sizeof(buf_), "Last OTA: %s", last_diag.c_str());
            log(buf_);
        }
    #endif


    // Start in legacy protocol mode
    plaintext_protocol_.init([this] () {
        changeConfig(true);
    });
    SerialProtocol* current_protocol = &plaintext_protocol_;

    ProtocolChangeCallback protocol_change_callback = [this, &current_protocol] (uint8_t protocol) {
        switch (protocol) {
            case SERIAL_PROTOCOL_LEGACY:
                current_protocol = &plaintext_protocol_;
                break;
            case SERIAL_PROTOCOL_PROTO:
                current_protocol = &proto_protocol_;
                break;
            default:
                log("Unknown protocol requested");
                break;
        }
    };

    plaintext_protocol_.setProtocolChangeCallback(protocol_change_callback);
    proto_protocol_.setProtocolChangeCallback(protocol_change_callback);

    plaintext_protocol_.setOtaRequestCallback([this] () {
        ota_requested_ = true;
    });

    // Interface loop:
    while (1) {
        PB_SmartKnobState state;
        if (xQueueReceive(knob_state_queue_, &state, 0) == pdTRUE) {
            current_protocol->handleState(state);
        }

        current_protocol->loop();

        if (ota_requested_) {
            runOtaMode(); // Does not return; ends in ESP.restart()
        }

        std::string* log_string;
        while (xQueueReceive(log_queue_, &log_string, 0) == pdTRUE) {
            current_protocol->log(log_string->c_str());
            delete log_string;
        }

        updateHardware();

        delay(1);
    }
}

void InterfaceTask::log(const char* msg) {
    // Allocate a string for the duration it's in the queue; it is free'd by the queue consumer
    std::string* msg_str = new std::string(msg);

    // Put string in queue (or drop if full to avoid blocking)
    xQueueSendToBack(log_queue_, &msg_str, 0);
}

void InterfaceTask::changeConfig(bool next) {
    if (next) {
        current_config_ = (current_config_ + 1) % COUNT_OF(configs);
    } else {
        if (current_config_ == 0) {
            current_config_ = COUNT_OF(configs) - 1;
        } else {
            current_config_ --;
        }
    }
    
    char buf_[256];
    snprintf(buf_, sizeof(buf_), "Changing config to %d -- %s", current_config_, configs[current_config_].text);
    log(buf_);
    motor_task_.setConfig(configs[current_config_]);
}

void InterfaceTask::updateHardware() {
    // How far button is pressed, in range [0, 1]
    float press_value_unit = 0;

    #if SK_ALS
        const float LUX_ALPHA = 0.005;
        static float lux_avg;
        float lux = veml.readLux();
        lux_avg = lux * LUX_ALPHA + lux_avg * (1 - LUX_ALPHA);
        static uint32_t last_als;
        if (millis() - last_als > 1000) {
            snprintf(buf_, sizeof(buf_), "millilux: %.2f", lux*1000);
            ble_task_.updateLux(lux*1000);
            log(buf_);
            last_als = millis();
        }
    #endif

    #if PIN_BUTTON_NEXT
        if (digitalRead(PIN_BUTTON_NEXT) == HIGH) {
            ble_task_.updateButton(false);
        }
        else{
            ble_task_.updateButton(true);
        }
    #endif

    
    #if SK_BLE
        if(ble_task_.otaRequested()){
            ota_requested_ = true;
        }
        if(ble_task_.calibrateRequested()){
            log("Starting motor calibration from BT");
            motor_task_.runCalibration();
        }
        if(ble_task_.hasInputFromBT()){
            if(ble_task_.hasNewMotorProfile()){
                PB_SmartKnobConfig profile = ble_task_.getMotorProfile();
                
                char buf_[256];
                snprintf(buf_, sizeof(buf_), "Changing profile from BT -- %s", profile.text);
                log(buf_);
                motor_task_.setConfig(profile);
            }
            if(ble_task_.hasNewMotorConfig()){
                MotorConfig config = ble_task_.getMotorConfig();

                char buf_[256];
                snprintf(buf_, sizeof(buf_), "Changing config from BT");
                log(buf_);
                motor_task_.setMotorConfig(config);
            }
            if(ble_task_.hasNewActuationPoint()){
                press_actuation_ = ble_task_.getActuationPoint();
                preferences_.putFloat("actuation", press_actuation_);
                ble_task_.updateActuation(press_actuation_);

                char buf_[256];
                snprintf(buf_, sizeof(buf_), "Actuation point set from BT: %.2f", press_actuation_);
                log(buf_);
            }
        }
    #endif

    #if SK_STRAIN
        if (scale.wait_ready_timeout(100)) {
            int32_t reading = scale.read();

            static uint32_t last_reading_display;
            if (millis() - last_reading_display > 1000) {
                snprintf(buf_, sizeof(buf_), "HX711 reading: %d", reading);
                ble_task_.updateScale(reading);
                log(buf_);
                last_reading_display = millis();
            }

            // TODO: calibrate and track (long term moving average) zero point (lower); allow calibration of set point offset
            const int32_t lower = -400000;
            const int32_t upper = 500000; // original value: 1800000
            // Ignore readings that are way out of expected bounds
            if (reading >= lower - (upper - lower) && reading < upper + (upper - lower)*2) {
                long value = CLAMP(reading, lower, upper);
                press_value_unit = 1. * (value - lower) / (upper - lower);
                ble_task_.updatePressure(press_value_unit);

                static bool pressed;
                if (!pressed && press_value_unit > press_actuation_) {
                    motor_task_.playHaptic(true);
                    ble_task_.updateScale(true);
                    pressed = true;
                    // changeConfig(true);
                } else if (pressed && press_value_unit < press_actuation_ / 3) {
                    motor_task_.playHaptic(false);
                    ble_task_.updateScale(false);
                    pressed = false;
                }
            }
        } else {
            log("HX711 not found.");

            #if SK_LEDS
                for (uint8_t i = 0; i < NUM_LEDS; i++) {
                    leds[i] = CRGB::Red;
                }
                FastLED.show();
            #endif
        }
    #endif

    uint16_t brightness = UINT16_MAX;
    // TODO: brightness scale factor should be configurable (depends on reflectivity of surface)
    #if SK_ALS
        brightness = (uint16_t)CLAMP(lux_avg * 13000, (float)1280, (float)UINT16_MAX);
    #endif

    #if SK_DISPLAY
        display_task_->setBrightness(brightness); // TODO: apply gamma correction
    #endif

    #if SK_LEDS
        for (uint8_t i = 0; i < NUM_LEDS; i++) {
            leds[i].setHSV(200 * press_value_unit, 255, brightness >> 8);

            // Gamma adjustment
            leds[i].r = dim8_video(leds[i].r);
            leds[i].g = dim8_video(leds[i].g);
            leds[i].b = dim8_video(leds[i].b);
        }
        FastLED.show();
    #endif
}

#if SK_LEDS
static void setAllLeds(const CRGB& color) {
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        leds[i] = color;
    }
    FastLED.show();
}
#endif

void InterfaceTask::otaDiagAppend(const char* fmt, ...) {
    size_t len = strlen(ota_diag_);
    if (len >= sizeof(ota_diag_) - 2) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(ota_diag_ + len, sizeof(ota_diag_) - len, fmt, args);
    va_end(args);
}

// Tears down BLE, brings up WiFi + ArduinoOTA + a TCP console (plaintext protocol
// on OTA_CONSOLE_PORT). Motor task keeps running, so the console stays interactive
// ('C' calibrate, <Space> config change). Never returns: exits via ESP.restart(),
// which boots back into normal BLE mode since nothing here is persisted.
//
// LED feedback: blue = scanning/joining WiFi, cyan = ready to flash,
// green fill = flash progress, magenta blinks = WiFi failed (reboots to normal).
// A compact diagnostic of the attempt is saved to NVS and reported over BLE
// after reboot (shows as "Last OTA" in the web app).
void InterfaceTask::runOtaMode() {
    ota_requested_ = false;
    ota_diag_[0] = 0;
    stream_.println("OTA MODE: starting");

    #if SK_BLE
        // Stop the BLE task before tearing down the stack it uses
        vTaskSuspend(ble_task_.getHandle());
        delay(50);
        BLEDevice::deinit(false);
        stream_.println("OTA MODE: BLE stopped");
    #endif

    #if SK_LEDS
        setAllLeds(CRGB::Blue);
    #endif

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(OTA_HOSTNAME);
    // EE regulatory domain (channels 1-13) and no power save: Nest WiFi Pro's 2.4GHz
    // radio may sit on ch 12/13, which the default country config scans poorly.
    wifi_country_t country = {"EE", 1, 13, 78, WIFI_COUNTRY_POLICY_MANUAL};
    esp_wifi_set_country(&country);
    esp_wifi_set_ps(WIFI_PS_NONE);

    // Tri-band mesh APs (WPA3 transition + band steering) are flaky with this old
    // WiFi stack: scan explicitly, pin the join to the strongest 2.4GHz BSSID, retry.
    bool wifi_connected = false;
    for (uint8_t attempt = 1; attempt <= 3 && !wifi_connected; attempt++) {
        // A previous attempt's join may still be in progress; scanning then fails with -2
        WiFi.disconnect(true);
        delay(500);
        snprintf(buf_, sizeof(buf_), "OTA MODE: scanning (attempt %d/3)...", attempt);
        stream_.println(buf_);
        int16_t n = WiFi.scanNetworks(false, true);
        int32_t best_rssi = -127;
        int32_t channel = 0;
        uint8_t bssid[6];
        for (int16_t i = 0; i < n; i++) {
            snprintf(buf_, sizeof(buf_), "  %s ch%d %ddBm", WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i));
            stream_.println(buf_);
            if (WiFi.SSID(i) == WIFI_SSID && WiFi.RSSI(i) > best_rssi) {
                best_rssi = WiFi.RSSI(i);
                channel = WiFi.channel(i);
                memcpy(bssid, WiFi.BSSID(i), 6);
            }
        }
        WiFi.scanDelete();

        if (channel > 0) {
            snprintf(buf_, sizeof(buf_), "OTA MODE: joining ch%d (%ddBm)", channel, best_rssi);
            stream_.println(buf_);
            otaDiagAppend("a%d:ch%d/%ddBm ", attempt, channel, best_rssi);
            WiFi.begin(WIFI_SSID, WIFI_PASS, channel, bssid);
        } else {
            stream_.println("OTA MODE: SSID not in scan, blind join attempt");
            otaDiagAppend("a%d:no-ap(%d nets) ", attempt, n);
            WiFi.begin(WIFI_SSID, WIFI_PASS);
        }

        uint32_t wifi_start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - wifi_start < OTA_WIFI_CONNECT_TIMEOUT_MS) {
            delay(100);
        }
        wifi_connected = WiFi.status() == WL_CONNECTED;
        if (!wifi_connected) {
            otaDiagAppend("tmo ");
        }
    }
    if (!wifi_connected) {
        stream_.println("OTA MODE: WiFi failed after 3 attempts, rebooting to normal mode");
        otaDiagAppend("FAIL");
        preferences_.putString("ota_diag", ota_diag_);
        #if SK_LEDS
            for (uint8_t b = 0; b < 3; b++) {
                setAllLeds(CRGB::Magenta);
                delay(250);
                setAllLeds(CRGB::Black);
                delay(250);
            }
        #endif
        delay(100);
        ESP.restart();
    }
    snprintf(buf_, sizeof(buf_), "OTA MODE: WiFi connected, IP %s", WiFi.localIP().toString().c_str());
    stream_.println(buf_);
    otaDiagAppend("OK %s", WiFi.localIP().toString().c_str());
    preferences_.putString("ota_diag", ota_diag_);

    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.onStart([this] () {
        ota_in_progress_ = true;
        stream_.println("OTA: update starting");
        otaDiagAppend(" flash-started");
        preferences_.putString("ota_diag", ota_diag_);
    });
    ArduinoOTA.onProgress([this] (unsigned int progress, unsigned int total) {
        #if SK_LEDS
            uint8_t lit = total > 0 ? (uint8_t)((uint64_t)progress * NUM_LEDS / total) : 0;
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                leds[i] = i < lit ? CRGB::Green : CRGB::Blue;
            }
            FastLED.show();
        #endif
    });
    ArduinoOTA.onEnd([this] () {
        stream_.println("OTA: done, rebooting");
    });
    ArduinoOTA.onError([this] (ota_error_t error) {
        ota_in_progress_ = false;
        snprintf(buf_, sizeof(buf_), "OTA: error %u", error);
        stream_.println(buf_);
    });
    ArduinoOTA.begin(); // Also registers mDNS as OTA_HOSTNAME.local

    tcp_stream_.begin();
    tcp_protocol_.init([this] () {
        changeConfig(true);
    });
    snprintf(buf_, sizeof(buf_), "OTA MODE: ready -- flash via espota, console on tcp port %d", OTA_CONSOLE_PORT);
    stream_.println(buf_);

    #if SK_LEDS
        setAllLeds(CRGB::Cyan); // ready to flash
    #endif

    uint32_t last_activity = millis();
    while (1) {
        ArduinoOTA.handle();
        tcp_stream_.loop();

        PB_SmartKnobState state;
        if (xQueueReceive(knob_state_queue_, &state, 0) == pdTRUE) {
            tcp_protocol_.handleState(state);
        }

        tcp_protocol_.loop();

        std::string* log_string;
        while (xQueueReceive(log_queue_, &log_string, 0) == pdTRUE) {
            tcp_protocol_.log(log_string->c_str());
            plaintext_protocol_.log(log_string->c_str());
            delete log_string;
        }

        if (tcp_stream_.hasClient() || ota_in_progress_) {
            last_activity = millis();
        }
        if (millis() - last_activity > OTA_IDLE_TIMEOUT_MS) {
            stream_.println("OTA MODE: idle timeout, rebooting to normal mode");
            otaDiagAppend(" idle-tmo");
            preferences_.putString("ota_diag", ota_diag_);
            delay(100);
            ESP.restart();
        }

        // Heartbeat with stack headroom so a starved task is diagnosable from the console
        static uint32_t last_heartbeat;
        if (millis() - last_heartbeat > 10000) {
            last_heartbeat = millis();
            snprintf(buf_, sizeof(buf_), "OTA MODE: alive, stack headroom %d bytes", uxTaskGetStackHighWaterMark(NULL));
            stream_.println(buf_);
            tcp_protocol_.log(buf_);
        }

        // Discovery beacon: broadcast our IP so tooling doesn't depend on mDNS
        // (stale mDNS caches have pointed smartknob.local at the wrong host)
        static uint32_t last_beacon;
        if (millis() - last_beacon > 2000) {
            last_beacon = millis();
            static WiFiUDP beacon_udp;
            beacon_udp.beginPacket(IPAddress(255, 255, 255, 255), OTA_BEACON_PORT);
            beacon_udp.print("SMARTKNOB-OTA ");
            beacon_udp.print(WiFi.localIP().toString());
            beacon_udp.endPacket();
        }

        delay(1);
    }
}
