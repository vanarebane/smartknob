"use strict";

// SmartKnob I/O:

// Sensor reads
//  *Ambient
//  *Gyro (firmare not implemented)
//  *Push
//  *Proximity (firmare not implemented)

// Interaciton event triggers
//  *Button change
//  *Position change

// Set config / calls
//  *Motor/Haptic
//  *Motor calibration
//  *Push
//  *Lights (firmare not implemented)

// Hardware manipulation
//  *Haptic
//  *Speaker (firmare not implemented)

// Value event triggers (set up a event to be called at certain level)
//  *Ambient (firmare not implemented)
//  *Proximity (firmare not implemented)

const SmartKnob = class {
    debug = true;

    // Find devices, filtered by this prefix
    ble_nameprefix = "SmartKnob_";

    ble_connected = false;

    debug_messages = {
        'disconnected': "Disconnected",
        'connecting': "Connecting",
        'connected': "Connected",
        'tryagain': "Try again",
        'error': "Error",
    };

    serviceUuid = 0x0001; // Primary service

    // As values here, but the code accepts first UUID that has notify and write property 
    TX_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"; // Notify
    RX_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"; // Write

    notify_tx_Characteristics = false;
    write_rx_Characteristics = false;

    useSingleCharacteristics = true;

    // Not used in this demo
    characteristicUuids = {
        'ambient': {
            'uuid': "0000420f-0000-1000-8000-00805f9b34fb",
            'functions': ["read", "write", "notify", "indicate"],
        },
        'scale': {
            'uuid': "e65dc16e-fb3e-4800-ad86-f78485cc65c2",
            'functions': ["read", "notify", "indicate"],
        },
        'button': {
            'uuid': "d769e5b4-601c-4506-b1da-29f64069869d",
            'functions': ["read", "notify", "indicate"],
        },
        'position': {
            'uuid': "602f75a0-697d-4690-8dd5-fa52781446d1",
            'functions': ["read", "write", "notify", "indicate"],
        },
        'lights': {
            'uuid': "0000424f-0000-1000-8000-00805f9b34fb",
            'functions': ["read", "notify", "indicate"],
        },
        'haptic': {
            'uuid': "0000425f-0000-1000-8000-00805f9b34fb",
            'functions': ["write"],
        },
        'config': {
            'uuid': "0000426f-0000-1000-8000-00805f9b34fb",
            'functions': ["read", "write", "notify", "indicate"],
        },
    };

    constructor() {
        window.addEventListener("onunload", this.disconnect());
    }

    // Function must be called from user interaction. Cannot be called directly.
    async connect() {
        navigator.bluetooth.addEventListener(
            "availabilitychanged",
            this.onavailabilitychanged
        );

        this.notify_tx_Characteristics = false;
        this.write_rx_Characteristics = false;

        try {
            this.eventDisconnected(this.debug_messages.connecting, "processing");
            this.log("Requesting Bluetooth Device...");
            const device = await navigator.bluetooth.requestDevice({
                    filters: [{ namePrefix: this.ble_nameprefix }],
                    optionalServices: [this.serviceUuid],
            });

            device.addEventListener('gattserverdisconnected', this.eventDisconnected);

            this.log("Connecting to GATT Server...");
            const server = await device.gatt.connect();

            this.log("Getting Service...");
            const service = await server.getPrimaryService(this.serviceUuid);

            this.log("Getting List of Characteristics...");
            let serviceCharacteristicsList = await service.getCharacteristics();
            this.log(serviceCharacteristicsList);

            for (let i = 0; i < serviceCharacteristicsList.length; i++) {
                let charac = serviceCharacteristicsList[i];

                if (charac.properties.notify && !this.notify_tx_Characteristics) {
                    console.log("startNotifications on " + charac.uuid);
                    this.notify_tx_Characteristics = charac; //await service.getCharacteristic(charac.uuid);

                    console.log(this.notify_tx_Characteristics);
                    // let value = await this.serviceCharacteristics[i].readValue()
                    // console.log("value: "+ value)
                    await this.notify_tx_Characteristics.startNotifications();
                    this.log("> Notifications started on " + charac.uuid);
                    this.notify_tx_Characteristics.addEventListener("characteristicvaluechanged", this.handleCombinedNotifications);
                }

                if (charac.properties.write && !this.write_rx_Characteristics) {
                    console.log("will Write on " + charac.uuid);
                    this.write_rx_Characteristics = charac; //await service.getCharacteristic(charac.uuid);
                    console.log(this.write_rx_Characteristics);
                }
            }
            
            this.eventConnected();
        } 
        catch (error) {
            // NetworkError: GATT Server is disconnected. Cannot retrieve services. (Re)connect first with `device.gatt.connect`.
            this.log("! BT Connection error" + error);
            this.eventDisconnected(this.debug_messages.disconnected, "error");
        }
    }

    // Called by event when BT disconnects
    disconnect() {
        if (this.serviceCharacteristic) {
            navigator.bluetooth.removeEventListener("availabilitychanged");

            this.serviceCharacteristics.forEach(async (charac, i) => {
                this.charac
                .stopNotifications()
                .then((_) => {
                    this.log("> Notifications stopped");
                    this.charac.removeEventListener("characteristicvaluechanged", handleNotifications);
                })
                .catch((error) => {
                    if (error == "NotFoundError: User cancelled the requestDevice() chooser.") {
                        this.eventDisconnected(this.debug_messages.tryagain);
                    }
                    //NetworkError: GATT operation already in progress.
                    //NotSupportedError: GATT operation failed for unknown reason.
                    //NetworkError: Failed to execute 'writeValue' on 'BluetoothRemoteGATTCharacteristic': GATT Server is disconnected. Cannot perform GATT operations. (Re)connect first with `device.gatt.connect`.
                    //NetworkError: GATT Server is disconnected. Cannot retrieve services. (Re)connect first with `device.gatt.connect`.
                    else {
                        console.log("! BT error: " + error);
                        this.eventDisconnected(this.debug_messages.error, "error");
                    }
                });
            });
        }
    }

    // Handle notifications that are keyd by the first byte
    handleCombinedNotifications(event) {
        let buffer = event.target.value;

        let key = buffer.getUint8(0);

        let value = 0;
        if(key == 4 || key == 7){
            value =
            buffer.getUint8(1) +
            buffer.getUint8(2) * 256 +
            buffer.getUint8(3) +
            buffer.getUint8(4) * 256;
            if (buffer.getUint8(3) > 0) {
                // if last two last bits have any value, the value is in negative
                value = value - 131071;
            }
        }
        else if(key !== 1 || key == 3){
            for (let i = 1; i < buffer.byteLength; i++) {
                value += i > 1 ? buffer.getUint8(i) * (256 * (i - 1)) : buffer.getUint8(i);
            }
        }

        // let response = bufferToString(buffer);
        // console.log(buffer, key, response, value);

        switch (key) { // First key byte for the type of value
            case 1: // Button change
                document.dispatchEvent(new CustomEvent("handleButtonNotifications", {
                    detail: { value: buffer.getUint8(1) == 1 ? true : false },
                }));
                break;

            case 2: // Scale value
                document.dispatchEvent(new CustomEvent("handleScaleNotifications", {
                    detail: { value: parseFloat(value) },
                }));
                break;

            case 3: // Push-down change
                document.dispatchEvent(new CustomEvent("handlePushNotifications", {
                    detail: { value: buffer.getUint8(1) == 1 ? true : false },
                }));
                break;

            case 4: // Position
                document.dispatchEvent(new CustomEvent("handlePositionNotifications", {
                    detail: { value: parseFloat(value) },
                }));
                break;

            case 5: // New position set
                document.dispatchEvent(new CustomEvent("handlePositionSetNotifications", {
                    detail: { value: parseFloat(value - 1) },
                }));
                break;

            case 6: // Lux value
                document.dispatchEvent(new CustomEvent("handleLuxNotifications", {
                    detail: { value: parseFloat(value - 1) },
                }));
                break;

            case 7: // Sub positions
                // let response = bufferToString(buffer);
                // console.log(buffer, buffer.byteLength, response, value);

                document.dispatchEvent(new CustomEvent("handlePositionSubNotifications", {
                    detail: { value: parseFloat(value - 1)/100 },
                }));
                break;
            }
    }

    // Prepares write command for profile specific
    async sendProfile(profile) {
        return await this.sendWrite("SP", profile.join(","));
    }

    // Prepares write command for profile specific
    async sendConfig(config) {
        return await this.sendWrite("CM", config.join(","));
    }
    
    // Sends write command to BT
    async sendWrite(cmd, value) {
        // try{
        let msg = sk.stringToBuffer(cmd + "+" + value);
        if (this.write_rx_Characteristics) {
            await this.write_rx_Characteristics.writeValue(msg);
            return true;
        } else {
            this.eventDisconnected()
            return false;
        }
    }

    // Handle notifications separated by UUID
    handleNotifications(event) {
        console.log(event.currentTarget.uuid);

        let buffer = event.target.value;

        // Display raw hex
        // let a = [];
        // for (let i = 0; i < buffer.byteLength; i++) {
        //     a.push('0x' + ('00' + buffer.getUint8(i).toString(16)).slice(-2));
        // }
        // console.log('>> ' + a.join(' '));

        // var data = new Int32Array(buffer);
        //let data = bufferToString(value)
        // let data = new DataView(buffer, 0)
        //let data = new Uint8Array(buffer)

        switch (event.currentTarget.uuid) {
            // Position
            case SmartKnob.characteristicUuids.position.uuid: {
                let value =
                buffer.getUint8(0) +
                buffer.getUint8(1) * 256 +
                buffer.getUint8(2) * 256 +
                buffer.getUint8(3) * 256;
                if (buffer.getUint8(2) > 0) {
                value = value - 196096;
                }

                document.dispatchEvent(
                new CustomEvent("handlePositionNotifications", {
                    detail: { value: value },
                })
                );
                break;
            }
            // Scale
            case SmartKnob.characteristicUuids.scale.uuid: {
                let value = buffer.getUint8(0);

                document.dispatchEvent(
                new CustomEvent("handleScaleNotifications", {
                    detail: { value: value },
                })
                );
                break;
            }
        }
    }

    onavailabilitychanged(event) {
        this.log("Availabiliy changed: " + event.value);
    }

    // When BT connects, this function is called that dispatches the isDisconnected
    eventConnected() {
        console.log("connected");
        this.ble_connected = true;
        document.dispatchEvent(new CustomEvent("isConnected", {detail: { message: this.debug_messages.connected, type: "success" },}));
    }

    // When BT disconnects, this function is called that dispatches the isDisconnected
    eventDisconnected(msg=this.debug_messages.disconnected, cls = "") {
        this.ble_connected = false;
        msg = (typeof msg !== "string") ? "Disconnected" : msg;
        document.dispatchEvent(new CustomEvent("isDisconnected", { detail: { message: msg, type: cls } }));
    }

    // Function to add event listener and log who is listening
    addListener(eventName, handler) {
        // Register here that there is a new listener
        // this.log(`now listening for ${eventName}`);
        document.addEventListener(eventName, handler);
    }
    // Function to add event listener and log who is listening
    removeListener(eventName, handler) {
        // Deregister here that there is a new listener
        document.removeEventListener(eventName, handler);
        // this.log(`${element} is removed from listening for ${eventName}`);
    }


    // HELPER FUNCTIONS

    stringToBuffer(str) {
        let Len = str.length,
        resPos = -1;
        // The Uint8Array's length must be at least 3x the length of the string because an invalid UTF-16
        //  takes up the equivalent space of 3 UTF-8 characters to encode it properly.
        let resArr = new Uint8Array(Len * 3);
        for (let point = 0, nextcode = 0, i = 0; i !== Len; ) {
        point = str.charCodeAt(i);
        i += 1;
        if (point >= 0xd800 && point <= 0xdbff) {
            if (i === Len) {
            resArr[(resPos += 1)] = 0xef; /*0b11101111*/
            resArr[(resPos += 1)] = 0xbf; /*0b10111111*/
            resArr[(resPos += 1)] = 0xbd; /*0b10111101*/
            break;
            }
            // https://mathiasbynens.be/notes/javascript-encoding#surrogate-formulae
            nextcode = str.charCodeAt(i);
            if (nextcode >= 0xdc00 && nextcode <= 0xdfff) {
            point = (point - 0xd800) * 0x400 + nextcode - 0xdc00 + 0x10000;
            i += 1;
            if (point > 0xffff) {
                resArr[(resPos += 1)] = (0x1e /*0b11110*/ << 3) | (point >>> 18);
                resArr[(resPos += 1)] =
                (0x2 /*0b10*/ << 6) | ((point >>> 12) & 0x3f); /*0b00111111*/
                resArr[(resPos += 1)] =
                (0x2 /*0b10*/ << 6) | ((point >>> 6) & 0x3f); /*0b00111111*/
                resArr[(resPos += 1)] =
                (0x2 /*0b10*/ << 6) | (point & 0x3f); /*0b00111111*/
                continue;
            }
            } else {
            resArr[(resPos += 1)] = 0xef; /*0b11101111*/
            resArr[(resPos += 1)] = 0xbf; /*0b10111111*/
            resArr[(resPos += 1)] = 0xbd; /*0b10111101*/
            continue;
            }
        }
        if (point <= 0x007f) {
            resArr[(resPos += 1)] = (0x0 /*0b0*/ << 7) | point;
        } else if (point <= 0x07ff) {
            resArr[(resPos += 1)] = (0x6 /*0b110*/ << 5) | (point >>> 6);
            resArr[(resPos += 1)] =
            (0x2 /*0b10*/ << 6) | (point & 0x3f); /*0b00111111*/
        } else {
            resArr[(resPos += 1)] = (0xe /*0b1110*/ << 4) | (point >>> 12);
            resArr[(resPos += 1)] =
            (0x2 /*0b10*/ << 6) | ((point >>> 6) & 0x3f); /*0b00111111*/
            resArr[(resPos += 1)] =
            (0x2 /*0b10*/ << 6) | (point & 0x3f); /*0b00111111*/
        }
        }
        return resArr.subarray(0, resPos + 1);
    }

        log() {
            if (this.debug) console.log(...arguments);
        }
    };

    function bufferToString(data) {
        if (!("TextDecoder" in window))
            alert("Sorry, this browser does not support TextDecoder...");
        var enc = new TextDecoder("utf-8");
        var arr = new Uint8Array(data.buffer);
        return enc.decode(arr);
    }
