$ = function() {
    var wellData = [];
    var wellDataGPM = [];
    var wellStartTime = 0;
    var wellStartTotal = 0;
    var wellEndTotal = 0;
    var wellEndTime = 0;
    var wellMaxGPM = 0;

    const gpmColor = "#fe45cd";
    const tgColor = "#3e95cd";

    fetch('welldeltadata.json').then(response => {
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        return response.json();
    }).then(wellDeltaData => {

        let runningTime = 0;
        let runningTotal = 0;
        for (i = 0; i < wellDeltaData.length; i++) {
            let v = wellDeltaData[i];
            if (v < 0) { // (re)start: read -time, -total
                let t = -v;
                let g = -wellDeltaData[++i];
                if (wellStartTime == 0) wellStartTime = t;
                if (wellStartTotal == 0) wellStartTotal = g;

                if (g == 0) g = runningTotal;

                wellData.push({
                    x: t * 1000,
                    y: g + .1
                });

                runningTime = t;
                runningTotal = g;
                wellEndTime = t;
                wellEndTotal = g;
            } else {
                let t = runningTime + v;
                let g = runningTotal + 1;

                wellData.push({
                    x: t * 1000,
                    y: g
                });

                runningTime = t;
                runningTotal = g;
                wellEndTime = t;
                wellEndTotal = g;
            }
        }

        // Compute Gallons per minute, approximately.
        let prev_x = 0;
        let prev_y = 0;
        for (let i = 0; i < wellData.length; i++) {
            let e = wellData[i];
            if (prev_x != 0) {
                let delta_x = e.x - prev_x;
                let delta_y = e.y - prev_y;
                let gpm = (60 * 1000.0 * delta_y) / delta_x;

                wellDataGPM.push({
                    x: e.x,
                    y: gpm
                });

                if (gpm > wellMaxGPM) wellMaxGPM = gpm;
            }

            prev_x = e.x;
            prev_y = e.y;
        }

        // Configure chart
        const ctx = document.getElementById('myChart').getContext('2d');
        const myChart = new Chart(ctx, {
            data: {
                datasets: [{
                    type: 'bar',
                    label: "Total gallons",
                    backgroundColor: tgColor,
                    barThickness: 'flex',
                    barPercentage: 1,
                    categoryPercentage: 1,
                    spanGaps: true,
                    data: wellData,
                    order: 2,
                }, {
                    type: 'line',
                    label: "gpm",
                    backgroundColor: gpmColor,
                    borderColor: gpmColor,
                    fill: false,
                    spanGaps: true,
                    data: wellDataGPM,
                    order: 1,
                    yAxisID: 'y-axis-right',
                }]
            },
            options: {
                legend: {
                    display: false
                },
                layout: {
                    padding: {
                        bottom: 80
                    }
                },
                scales: {
                    x: {
                        type: 'time',
                        time: {
                            minUnit: 'second',
                            displayFormats: {
                                'second': 'MMM dd HH:mm:ss',
                                'minute': 'MMM dd HH:mm:ss',
                                'hour': 'MMM dd HH:mm:ss',
                            }
                        },
                        min: wellStartTime * 1000,
                        max: wellEndTime * 1000,
                        ticks: {
                            //source: 'data',
                            minRotation: 45,
                        },
                    },
                    'y-axis-left': {
                        type: 'linear',
                        position: 'left',
                        beginAtZero: false,
                        min: wellStartTotal,
                        max: wellEndTotal,
                        ticks: {
                            stepSize: 1, // integers please
                        },
                    },
                    'y-axis-right': {
                        type: 'linear',
                        position: 'right',
                        beginAtZero: true,
                        min: 0,
                        max: wellMaxGPM,
                        grid: {
                            drawOnChartArea: false,
                        },
                        ticks: {
                            color: gpmColor,
                        },
                    },
                },
                title: {
                    display: true,
                    text: 'Finca gallons pumped'
                },
                plugins: {
                    zoom: {
                        zoom: {
                            wheel: {
                                enabled: true
                            },
                            pinch: {
                                enabled: true
                            },
                            mode: 'x',
                            limits: {
                                x: {
                                    minRange: 60000 // one minute
                                }
                            },
                        },
                        pan: {
                            enabled: true,
                            mode: 'x',
                        },
                        limits: {
                            x: {
                                min: wellStartTime * 1000,
                                max: wellEndTime * 1000,
                            }
                        },
                    }
                }
            },
        });

    }).catch(error => {
        document.write('Error: ', error);
    });
}();
