function createGraph(can, tab, tit, idd) {

    // some data to be plotted
    let x_data = ["0.00", "1.00", "2.00", "3.00", "4.00", "5.00", "6.00", "7.00", "8.00", "9.00", "10.00"];
    let y_data_1 = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

    let activePoint = null;
    let canvas = can;
    let table = tab;
    let title = tit
    let id = idd
    let chart = null
    graph();

    // draw a line chart on the canvas context
    function graph() {
        // Draw a line chart with two data sets
        let ctx = canvas.getContext("2d");

        var gradient = ctx.createLinearGradient(0, 0, 0, 400)
        gradient.addColorStop(0, '#BF570099')
        gradient.addColorStop(1, '#FFFFFF33')

        const myChart = Chart.Line(ctx, {
            data: {
                labels: x_data,
                datasets: [
                    {
                        data: y_data_1,
                        label: "Data 1",
                        borderColor: "#BF5700",
                        fill: true,
                        tension: 0,
                        pointRadius: 5,
                        pointHoverRadius: 10,
                        backgroundColor: gradient

                    },
                ]
            },
            options: {

                title: {
                    display: true,
                    text: title
                },

                animation: {
                    duration: 200
                },
                legend: {
                    display: false
                },
                tooltips: {
                    mode: 'nearest'
                }
            }
        });

        chart = myChart

        addTable(table)

        // set pointer event handlers for canvas element
        canvas.onpointerdown = down_handler;
        canvas.onpointerup = up_handler;
        canvas.onpointermove = null;
    };

    function addTable(myTableDiv) {

        let table = document.createElement('TABLE');
        table.border = '1';
        table.id = id;

        let tableBody = table


        for (let i = 0; i < 11; i++) {
            let tr = document.createElement('TR');
            tr.id = title + i;
            tableBody.appendChild(tr);

            for (let j = 0; j < 2; j++) {
                let td = document.createElement('TD');
                td.id = title + i.toString() + j.toString();
                td.width = '75';
                td.appendChild(document.createTextNode(rnd(i)));

                if (j == 0 && (i == 0 || i == 10)) {
                    td.setAttribute("contenteditable", "true");
                    td.addEventListener('input', function () {
                        let fElem = Number(document.getElementById(title + "00").textContent)
                        let inc = (Number(document.getElementById(title + "100").textContent) - fElem) / 10
                        for (let k = 0; k < 11; k++) {
                            document.getElementById(title + k.toString() + "0").textContent = (rnd(fElem + inc * k)).toString()
                            chart.data.labels[k] = (rnd(fElem + inc * k))
                        }
                        chart.update();
                    });

                } else if (j == 1) {
                    td.setAttribute("contenteditable", "true");
                    td.addEventListener('input', function () {
                        let yValue = Number(td.textContent);
                        chart.data.datasets[0].data[i] = yValue;
                        chart.update();
                    });
                } else {
                    td.style.backgroundColor = "#CCCCCC"
                }

                tr.appendChild(td);
            }
        }
        try {
            myTableDiv.appendChild(table);
        } catch (e) { }

    }

    function down_handler(event) {
        // check for data point near event location
        const points = chart.getElementAtEvent(event, { intersect: false });
        if (points.length > 0) {
            // grab nearest point, start dragging
            activePoint = points[0];
            canvas.onpointermove = move_handler;
        };
    };

    function up_handler(event) {
        // release grabbed point, stop dragging
        activePoint = null;
        canvas.onpointermove = null;
    };

    function move_handler(event) {
        // locate grabbed point in chart data
        if (activePoint != null) {
            let data = activePoint._chart.data;
            let datasetIndex = activePoint._datasetIndex;

            // read mouse position
            const helpers = Chart.helpers;
            let position = helpers.getRelativePosition(event, chart);

            // convert mouse position to chart y axis value 
            let chartArea = chart.chartArea;
            let yAxis = chart.scales["y-axis-0"];
            let yValue = rnd(map(position.y, chartArea.bottom, chartArea.top, yAxis.min, yAxis.max));

            // update y value of active data point
            data.datasets[datasetIndex].data[activePoint._index] = yValue;
            chart.update();
            tr = document.getElementById(title + activePoint._index.toString() + 1);
            tr.textContent = yValue;
        };
    };

    function rnd(num) {
        return num.toFixed(2);
    }

    // map value to other coordinate system
    function map(value, start1, stop1, start2, stop2) {
        return start2 + (stop2 - start2) * ((value - start1) / (stop1 - start1))
    };
}