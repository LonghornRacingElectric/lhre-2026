let current_pos, map, lines
let avg_latlngs = Array()

if (navigator.geolocation) {
    navigator.geolocation.getCurrentPosition((position) => {
        map = L.map('map').setView([position.coords.latitude, position.coords.longitude], 500)
        L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', {
            maxNativeZoom: 19, maxZoom: 23, attribution: '&copy; <a href="http://www.openstreetmap.org/copyright">OpenStreetMap</a>'
        }).addTo(map)
        map.setZoom(20)
        lines = L.featureGroup().addTo(map)
        current_pos = L.circleMarker([position.coords.latitude, position.coords.longitude], {radius: 2}).addTo(map)
        current_pos.setStyle({color: 'purple'})
        setInterval(current_location, 500)
    }, errorCallback, {enableHighAccuracy: true})
} else {
    const xhr = new XMLHttpRequest();
    let host_ip
    if ('{{ host_ip }}'.indexOf('local') === -1) {
        host_ip = 'https://{{ host_ip }}'
    } else {
        host_ip = 'http://{{ host_ip }}'
    }
    xhr.open('POST', host_ip + ':5000/gates', true)
    xhr.setRequestHeader('Content-Type', 'application/json; charset=UTF-8')
    xhr.send('Error: No geolocation available.\n')
}

function current_location() {
        navigator.geolocation.getCurrentPosition((position) => {
            current_pos.setLatLng(L.latLng(position.coords.latitude, position.coords.longitude))
        }, errorCallback, {enableHighAccuracy: true})
}

async function getLocation() {
    const loader = document.querySelector(".loader");
    setTimeout(() => loader.hidden = true, 10000);
    let avg_latlng = [0, 0]
    let msg = ''
    let temp_marks = L.featureGroup().addTo(map)
    for (let i = 0; i < 50; i++) {
        navigator.geolocation.getCurrentPosition((position) => {
            L.circleMarker([position.coords.latitude, position.coords.longitude], {radius: 2}).addTo(temp_marks)
            avg_latlng[0] += position.coords.latitude
            avg_latlng[1] += position.coords.longitude
            msg += '\n' + JSON.stringify({latitude: position.coords.latitude, longitude: position.coords.longitude})
            }, errorCallback, {enableHighAccuracy: true})
        document.getElementById("result").innerText = msg
        await new Promise(r => setTimeout(r, 200));
    }
    temp_marks.clearLayers()
    await sendLocation([avg_latlng[0] / 50., avg_latlng[1] / 50.])
}


function sendLocation(latlng) {
    console.log(avg_latlngs.length)
    if (avg_latlngs.length === 2) {
        avg_latlngs.shift()
        lines.clearLayers()
    }
    avg_latlngs.push(latlng)
    L.polyline(avg_latlngs, {color: 'red'}).addTo(lines)
    let avg_marker = L.circleMarker(latlng, {radius: 3, fillColor: 'green'}).addTo(map)
    avg_marker.setStyle({color: 'green'})

    const xhr = new XMLHttpRequest();
    let host_ip
    if ('{{ host_ip }}'.indexOf('local') === -1) {
        host_ip = 'https://{{ host_ip }}'
    } else {
        host_ip = 'http://{{ host_ip }}'
    }
    xhr.open('POST', host_ip + '/gates', true)
    xhr.setRequestHeader('Content-Type', 'application/json; charset=UTF-8')
    const body = JSON.stringify({
        latitude: latlng[0],
        longitude: latlng[1]
    })
    xhr.send(body)
    document.getElementById('result').innerText = 'Successfully sent:\n' + body
}
function errorCallback(error) {
    if (error.code === error.TIMEOUT) {

    }
    console.log('Errored out: ' + error.code)
    document.getElementById('result').innerText = 'Errored out: ' + error.code + '\n' + error.message
}