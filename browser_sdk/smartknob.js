
'use strict'

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

    debug = true

    ble_nameprefix = "SmartKnob_"

    ble_connected = false

    debug_messages = {
        'disconnected': "Disconnected",
        'connecting': "Connecting",
        'connected': "Connected",
        'tryagain': "Try again",
        'error': "Error"
    }

    // isConnected = new CustomEvent('isConnected', {detail: {'message': this.debug_messages.connected, 'type': "success"}})
    // isDisconnected = new CustomEvent('isDisconnected', {})

    serviceUuid = 0x340f // Primary service

    serviceCharacteristics = []

    characteristicUuids = {
        'ambient': {
            'uuid': "0000420f-0000-1000-8000-00805f9b34fb",
            'functions': ['read', 'write', 'notify', 'indicate']
        },
        'scale': {
            'uuid': "e65dc16e-fb3e-4800-ad86-f78485cc65c2",
            'functions': ['read', 'notify', 'indicate']
        },
        'button': {
            'uuid': "d769e5b4-601c-4506-b1da-29f64069869d",
            'functions': ['read', 'notify', 'indicate']
        },
        'position': {
            'uuid': "602f75a0-697d-4690-8dd5-fa52781446d1",
            'functions': ['read', 'write', 'notify', 'indicate']
        },
        'lights': {
            'uuid': "0000424f-0000-1000-8000-00805f9b34fb",
            'functions': ['read', 'notify', 'indicate']
        },
        'haptic': {
            'uuid': "0000425f-0000-1000-8000-00805f9b34fb",
            'functions': ['write']
        },
        'config': {
            'uuid': "0000426f-0000-1000-8000-00805f9b34fb",
            'functions': ['read', 'write', 'notify', 'indicate']
        },
    }

    constructor() {
        window.addEventListener('onunload', this.disconnect())
    }

    async connect(){
        
        navigator.bluetooth.addEventListener("availabilitychanged", this.onavailabilitychanged)
        
        // try {

            this.eventDisconnected(this.debug_messages.connecting, "processing")
            this.log('Requesting Bluetooth Device...');
            const device = await navigator.bluetooth.requestDevice({
                filters: [{"namePrefix": this.ble_nameprefix}],
                optionalServices: [this.serviceUuid]
            });

            this.log('Connecting to GATT Server...');
            const server = await device.gatt.connect();

            this.log('Getting Service...');
            const service = await server.getPrimaryService(this.serviceUuid);

            this.log('Getting List of Characteristics...');
            let serviceCharacteristicsList = await service.getCharacteristics()
            this.log(serviceCharacteristicsList)
 
            let i=0
            // for(let i=0; i<serviceCharacteristicsList.length; i++){
                let charac = serviceCharacteristicsList[i]

                console.log("startNotifications on "+charac.uuid)
                this.serviceCharacteristics[i] = charac //await service.getCharacteristic(charac.uuid);

                console.log(this.serviceCharacteristics[i])
                // let value = await this.serviceCharacteristics[i].readValue()
                // console.log("value: "+ value)

                await this.serviceCharacteristics[i].startNotifications();
                this.log('> Notifications started on '+charac.uuid);
                this.serviceCharacteristics[i].addEventListener('characteristicvaluechanged', this.handleNotifications);
            // }

            this.eventConnected()

            // log('Getting Service 2...');
            // const service2 = await server.getPrimaryService(serviceUuid2);

            // log('Getting Characteristic 2...');
            // powerCharacteristic = await service2.getCharacteristic(powerCharacteristicUuid);

            // await powerCharacteristic.startNotifications();
            // log('> Notifications started');
            // powerCharacteristic.addEventListener('characteristicvaluechanged', handlePwrNotifications);


            // document.querySelector('#writeButton').disabled = false;
            // log('Reading Alert Level...');
            // const value = await serviceCharacteristic.readValue();

            // log('> Alert Level: ' + getAlertLevel(value));
            
        // } catch(error) {
        //     // document.querySelector('#writeButton').disabled = true;
        //     // NetworkError: GATT Server is disconnected. Cannot retrieve services. (Re)connect first with `device.gatt.connect`.
        //     this.log('! BT Connection error' + error);
        //     this.eventDisconnected(this.debug_messages.tryagain, "error")
        // }
    }

    disconnect(){
        if(this.serviceCharacteristic){
            
            navigator.bluetooth.removeEventListener("availabilitychanged")

            this.serviceCharacteristics.forEach(async (charac, i) => {
                this.charac.stopNotifications()
                .then(_ => {
                    this.log('> Notifications stopped');
                    this.charac.removeEventListener('characteristicvaluechanged', handleNotifications);
                    })
                .catch(error => {
                    if(error == "NotFoundError: User cancelled the requestDevice() chooser."){
                        this.eventDisconnected(this.debug_messages.tryagain)
                    }
                    //NetworkError: GATT operation already in progress.
                    //NotSupportedError: GATT operation failed for unknown reason.
                    //NetworkError: Failed to execute 'writeValue' on 'BluetoothRemoteGATTCharacteristic': GATT Server is disconnected. Cannot perform GATT operations. (Re)connect first with `device.gatt.connect`.
                    //NetworkError: GATT Server is disconnected. Cannot retrieve services. (Re)connect first with `device.gatt.connect`.
                    else{
                        console.log('! BT error: ' + error);
                        this.eventDisconnected(this.debug_messages.error, "error")
                    }
                }); 
            })
        }
    }

    handleNotifications(event) {
        
        console.log(event.currentTarget.uuid)

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

        switch(event.currentTarget.uuid){
            case "602f75a0-697d-4690-8dd5-fa52781446d1":{
                let value = buffer.getUint8(0)+(buffer.getUint8(1)*256)+(buffer.getUint8(2)*256)+(buffer.getUint8(3)*256)
                if(buffer.getUint8(2) > 0){
                    value = (value-196096)
                }

                document.dispatchEvent(new CustomEvent('handlePositionNotifications', {detail: {'value': value}}))
                break;
            }
            case "0000421f-0000-1000-8000-00805f9b34fb":{
                let value = buffer.getUint8(0)

                document.dispatchEvent(new CustomEvent('handleScaleNotifications', {detail: {'value': value}}))
                break;
            }
        }


        // $('[name="position_cur"]').val(value)
        //console.log(value);
        //processNotificationState(data)
        //document.dispatchEvent(new CustomEvent('handleNotifications', {detail: {'value': value}}))
        
    }

    onavailabilitychanged(event){
        this.log("Availabiliy changed: "+event.value)
    }

    eventConnected(){
        console.log("connected")
        this.ble_connected = true
        document.dispatchEvent(new CustomEvent('isConnected', {detail: {'message': this.debug_messages.connected, 'type': "success"}}))
    }
    eventDisconnected(msg=this.debug_messages.disconnected, cls=""){
        this.ble_connected = false
        document.dispatchEvent(new CustomEvent('isDisconnected', {detail: {'message': msg, 'type': cls}}))
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
        
    bufferToString(data) {
        if (!("TextDecoder" in window))
            alert("Sorry, this browser does not support TextDecoder...");
        var enc = new TextDecoder("utf-8");
        var arr = new Uint8Array(data.buffer);
        return enc.decode(arr);
    }

    stringToBuffer(str) {
        let Len = str.length,
            resPos = -1
        // The Uint8Array's length must be at least 3x the length of the string because an invalid UTF-16
        //  takes up the equivalent space of 3 UTF-8 characters to encode it properly.
        let resArr = new Uint8Array(Len * 3)
        for (let point = 0, nextcode = 0, i = 0; i !== Len; ) {
            point = str.charCodeAt(i)
            i += 1
            if (point >= 0xd800 && point <= 0xdbff) {
                if (i === Len) {
                    resArr[(resPos += 1)] = 0xef /*0b11101111*/
                    resArr[(resPos += 1)] = 0xbf /*0b10111111*/
                    resArr[(resPos += 1)] = 0xbd /*0b10111101*/
                    break
                }
                // https://mathiasbynens.be/notes/javascript-encoding#surrogate-formulae
                nextcode = str.charCodeAt(i)
                if (nextcode >= 0xdc00 && nextcode <= 0xdfff) {
                    point = (point - 0xd800) * 0x400 + nextcode - 0xdc00 + 0x10000
                    i += 1
                    if (point > 0xffff) {
                        resArr[(resPos += 1)] = (0x1e /*0b11110*/ << 3) | (point >>> 18)
                        resArr[(resPos += 1)] =
                            (0x2 /*0b10*/ << 6) | ((point >>> 12) & 0x3f) /*0b00111111*/
                        resArr[(resPos += 1)] =
                            (0x2 /*0b10*/ << 6) | ((point >>> 6) & 0x3f) /*0b00111111*/
                        resArr[(resPos += 1)] =
                            (0x2 /*0b10*/ << 6) | (point & 0x3f) /*0b00111111*/
                        continue
                    }
                } else {
                    resArr[(resPos += 1)] = 0xef /*0b11101111*/
                    resArr[(resPos += 1)] = 0xbf /*0b10111111*/
                    resArr[(resPos += 1)] = 0xbd /*0b10111101*/
                    continue
                }
            }
            if (point <= 0x007f) {
                resArr[(resPos += 1)] = (0x0 /*0b0*/ << 7) | point
            } else if (point <= 0x07ff) {
                resArr[(resPos += 1)] = (0x6 /*0b110*/ << 5) | (point >>> 6)
                resArr[(resPos += 1)] =
                    (0x2 /*0b10*/ << 6) | (point & 0x3f) /*0b00111111*/
            } else {
                resArr[(resPos += 1)] = (0xe /*0b1110*/ << 4) | (point >>> 12)
                resArr[(resPos += 1)] =
                    (0x2 /*0b10*/ << 6) | ((point >>> 6) & 0x3f) /*0b00111111*/
                resArr[(resPos += 1)] =
                    (0x2 /*0b10*/ << 6) | (point & 0x3f) /*0b00111111*/
            }
        }
        return resArr.subarray(0, resPos + 1)
    }

    log(){
        if(this.debug) console.log(...arguments);
    }
}