//Encodes information into a .json file and publishes the data to mqtt
function encodeValues(timerStatus, updateTimerTime, updateIntTime, turnStatus, accelStatus, publishData, endFlag) {
    console.log("Encoder Triggered.") //TODO remove, debug
    //NOTE: passing null (or no value) means DO NOT UPDATE that element

    //Debug Statement for Config Image
    //console.log("Config image stores this prior to encode call: " + (config_image == null ? "null" : JSON.stringify(config_image)))

    //Create json object reflecting current state to be published
    const jsonData = {
        "timerRunning": (timerStatus != null) ? timerStatus : config_image.timerRunning,
        "timerEventTime": updateTimerTime ? Date.now() : config_image.timerEventTime,
        "timerInternalTime": updateIntTime ? watch.getTime() : config_image.timerInternalTime,
        "turnRunning" : (turnStatus != null) ? turnStatus : config_image.turnRunning,
        "turnStamp" : (turnStatus != null) ? watch.getTime() : config_image.turnStamp,
        "accelRunning" : (accelStatus != null) ? accelStatus : config_image.accelRunning,
        "accelStamp" : (accelStatus != null) ? watch.getTime() : config_image.accelStamp,
        "endFlag" : endFlag,
        "tables" : (config_image && config_image.tables) ? config_image.tables : makeEmptyTable()
    }

    console.log("Encoder Storing & Sending: " + JSON.stringify(jsonData))
    //Update the cached image
    config_image = jsonData

    if (publishData) {
        //Publish changes to MQTT state topic
        let message = new Paho.MQTT.Message(JSON.stringify(jsonData))
        message.destinationName = "config/event_sync"
        client.send(message)
    }

    function makeEmptyTable() {
        return {
            turnStarts: [],
            turnStops: [],
            turnNotes: [],
            accelStarts: [],
            accelStops: [],
            accelNotes: []
        }
    }
}