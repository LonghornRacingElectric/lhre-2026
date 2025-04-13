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
        "laps": (config_image && config_image.hasOwnProperty("laps")) ? config_image.laps : [],
        "endFlag" : endFlag,
        "tables" : (config_image && config_image.tables) ? config_image.tables : makeEmptyTable(),
        "lastSentClientId": myName,
        "selfDecodeAllowed": true
    }

    console.log("Encoder Storing & Sending: " + JSON.stringify(jsonData))
    //Update the cached image
    config_image = jsonData

    if (publishData) {

        //Update locally prior to publishing
        decodeValues(JSON.stringify(jsonData));

        //Disallow self decode (so no double processing occurs)
        jsonData.selfDecodeAllowed = false;

        //Send JSON data back to the server via HTTP POST request
        fetch('/update-event-sync', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(jsonData)
        })
        .then(response => response.json())
        .then(data => {
            console.log("State send successful:", data);
        })
        .catch(err => {
            console.error("Error sending event sync: ", err);
        });
    }

    function makeEmptyTable() {
        return {
            turnStarts: [],
            turnStops: [],
            turnNotes: [],
            accelStarts: [],
            accelStops: [],
            accelNotes: [],
        }
    }
}