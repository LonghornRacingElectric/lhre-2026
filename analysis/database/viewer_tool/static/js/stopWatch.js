function Stopwatch(elem) {

  let time = 0;
  let interval;
  let offset;

  function update() {
    if (this.isOn) {
      time += delta();
    }
    let formattedTime = timeFormatter(time);
    elem.textContent = formattedTime;
  }

  function delta() {
    let now = Date.now()
    let timePassed = now - offset;
    offset = now;
    return timePassed
  }

  this.getTime = function() {
    return time;
  }

  function timeFormatter(timeInMS) {
    let time = new Date(timeInMS);
    let minutes = time.getMinutes().toString();
    let seconds = time.getSeconds().toString();
    let ms = time.getMilliseconds().toString();

    if (minutes.length < 2) {
      minutes = '0' + minutes;
    }

    if (seconds.length < 2) {
      seconds = '0' + seconds;
    }

    while (ms.length < 3) {
      ms = '0' + ms;
    }
    return minutes + ':' + seconds + '.' + ms;
  };

  function loadTable(turn, time_start, time_stop, table, note) {
    let row = table.insertRow(-1);
    let participantCell = row.insertCell(0);
    let timeStartCell = row.insertCell(1);
    let timeStopCell = row.insertCell(2);
    let notesCell = row.insertCell(3);
    notesCell.contentEditable = "true";
    notesCell.innerHTML = (note) ? note : "";
    participantCell.innerHTML = turn;
    timeStartCell.innerHTML = time_start;
    timeStopCell.innerHTML = time_stop;
  };

  this.turnTableToJSON = function (table) {
    let data = [];
    for (let i = 1; i < table.rows.length; i++) {
      let tableRow = table.rows[i];
      let rowData = [];
      for (let j = 0; j < tableRow.cells.length; j++) {
        rowData.push(tableRow.cells[j].innerHTML);
      }
      data.push(rowData);
    }
    let xhr = new XMLHttpRequest();
    xhr.open('POST', 'http://localhost:5000/turn_data');
    xhr.setRequestHeader('Content-Type', 'application/json', 'charset=UTF-8');
    xhr.send(JSON.stringify(data));
  }

  this.tableToJSON = function (table, add) {
    let data = [];
    for (let i = 1; i < table.rows.length; i++) {
      let tableRow = table.rows[i];
      let rowData = [];
      for (let j = 0; j < tableRow.cells.length; j++) {
        rowData.push(tableRow.cells[j].innerHTML);
      }
      data.push(rowData);
    }
    let xhr = new XMLHttpRequest();
    xhr.open('POST', add);
    xhr.setRequestHeader('Content-Type', 'application/json', 'charset=UTF-8');
    xhr.send(JSON.stringify(data));
  }
  
  this.isOn = false;

  this.start = function () {
    if (!this.isOn) {
      interval = setInterval(update.bind(this), 10);
      offset = Date.now();
      this.isOn = true;
    }
  };

  this.startAt = function (timeStarted, internalWhenStarted) {
    if (!this.isOn) {
      interval = setInterval(update.bind(this), 10);
      time = internalWhenStarted
      offset = timeStarted;
      this.isOn = true;
    }
  }

  this.stop = function () {
    if (this.isOn) {
      clearInterval(interval);
      interval = null;
      this.isOn = false;
    }
  };

  this.stopAt = function(timeStopped) {
    time = timeStopped
    clearInterval(interval)
    interval = null
    this.isOn = false
    update()
  }

  let turn = 1;
  let isTurning = false;
  let startTime;

  this.isTurning = function() {
    return isTurning
  }

  this.turn = function() {
    if (isTurning) {
      let tempCurTime = document.getElementById('timer').textContent
      loadTable(turn, startTime, tempCurTime, document.getElementById('turn-table'));
      document.getElementById('turnButton').textContent = 'Start Turn'
      turn++;
      isTurning = false;
      //Push table row to tables in mqtt (ONLY on client that pushed button, when posted)
      config_image.tables.turnStarts.push(startTime)
      config_image.tables.turnStops.push(tempCurTime)

    } else {
      startTime = document.getElementById('timer').textContent;
      isTurning = true;
      document.getElementById('turnButton').textContent = 'End Turn'
    }
  };

  this.loadCustomTurn = function(start, end, note) {
    loadTable(turn, start, end, document.getElementById('turn-table'), note);
    turn++;
  }

  this.turnAt = function(timeTurned) {
    if (isTurning) {
      loadTable(turn, startTime, timeFormatter(timeTurned), document.getElementById('turn-table'));
      document.getElementById('turnButton').textContent = 'Start Turn'
      turn++;
      isTurning = false;
    } else {
      startTime = timeFormatter(timeTurned);
      isTurning = true;
      document.getElementById('turnButton').textContent = 'End Turn'
    }
  }

  let accel = 1;
  let isAccel = false;
  let startAccelTime;

  this.isAccel = function() {
    return isAccel
  }

  this.loadCustomAccel = function(start, end, note) {
    loadTable(accel, start, end, document.getElementById('accel-table'), note);
    accel++;
  }

  this.accel = function() {
    if (isAccel) {
      let tempCurTime = document.getElementById('timer').textContent
      loadTable(accel, startAccelTime, tempCurTime, document.getElementById('accel-table'));
      document.getElementById('accelButton').textContent = 'Start Acceleration'
      accel++;
      isAccel = false;
      //Push table row to tables in mqtt (ONLY on client that pushed button, when posted)
      config_image.tables.accelStarts.push(startAccelTime)
      config_image.tables.accelStops.push(tempCurTime)
    } else {
      startAccelTime = document.getElementById('timer').textContent;
      isAccel = true;
      document.getElementById('accelButton').textContent = 'End Acceleration'
    }
  };

  this.accelAt = function(accelTime) {
    if (isAccel) {
      loadTable(accel, startAccelTime, timeFormatter(accelTime), document.getElementById('accel-table'));
      document.getElementById('accelButton').textContent = 'Start Acceleration'
      accel++;
      isAccel = false;
    } else {
      startAccelTime = timeFormatter(accelTime);
      isAccel = true;
      document.getElementById('accelButton').textContent = 'End Acceleration'
    }
  };


}