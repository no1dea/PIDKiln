var ctx = document.getElementById('ProgramChart').getContext('2d');
var chart_update_id;

var chartColors = {
	red: 'rgb(232, 120, 120)',
	blue: 'rgb(107, 140, 174)',
	yel: 'rgb(212, 160, 90)',
	green: 'rgba(107, 140, 174, 0.18)'
};

Chart.defaults.global.defaultFontColor = '#9aa0a8';
Chart.defaults.global.defaultFontFamily = 'ui-monospace, Cascadia Mono, Consolas, monospace';

var axisDefaults = {
	gridLines: {
		color: 'rgba(61, 68, 80, 0.9)',
		zeroLineColor: 'rgba(61, 68, 80, 1)'
	},
	ticks: {
		fontColor: '#9aa0a8'
	},
	scaleLabel: {
		fontColor: '#c8ccd2'
	}
};

var config_with = {
	type: 'line',
	data: {
		datasets: [{
			yAxisID: 'temperature',
			backgroundColor: 'transparent',
			borderColor: chartColors.red,
			pointBackgroundColor: chartColors.red,
			tension: 0.1,
			fill: false
		     }, {
			yAxisID: 'temperature',
			backgroundColor: 'transparent',
			borderColor: chartColors.yel,
			pointBackgroundColor: chartColors.yel,
			tension: 0.1,
			fill: false
		     }, {
			yAxisID: 'watts',
			backgroundColor: chartColors.green,
			borderColor: 'transparent',
			fill: true
		     }, {
			label: 'Running program ~PROGRAM_NAME~',
			yAxisID: 'temperature',
			backgroundColor: 'transparent',
			borderColor: chartColors.blue,
			fill: false,
			tension: 0.1,
			data: [~CHART_DATA~]
		}]
	},
	plugins: [ChartDataSource],
	options: {
		legend: {
			labels: { fontColor: '#c8ccd2' }
		},
		title: {
			display: true,
			text: 'Set program: ~PROGRAM_NAME~ vs actual temperature',
			fontColor: '#e8eaed'
		},
		responsive: true,
		scales: {
			xAxes: [{
				type: 'time',
				time: {
					tooltipFormat: 'YYYY-MM-DD HH:mm',
					displayFormats: {
						millisecond: 'HH:mm:ss.SSS',
						second: 'HH:mm:ss',
						minute: 'HH:mm',
						hour: 'HH'
					}
				},
				gridLines: axisDefaults.gridLines,
				ticks: axisDefaults.ticks,
				scaleLabel: {
					display: true,
					labelString: 'Date',
					fontColor: axisDefaults.scaleLabel.fontColor
				}
			}],
			yAxes: [{
				id: 'temperature',
				gridLines: axisDefaults.gridLines,
				ticks: axisDefaults.ticks,
				scaleLabel: {
					display: true,
					labelString: 'Temperature (C)',
					fontColor: axisDefaults.scaleLabel.fontColor
				}
			},{
				id: 'watts',
				position: 'right',
				gridLines: {
					display: false
				},
				ticks: {
					bounds: 'data',
					beginAtZero: true,
					fontColor: axisDefaults.ticks.fontColor
				},
				scaleLabel: {
					display: true,
					labelString: 'Watts',
					fontColor: axisDefaults.scaleLabel.fontColor
				}
			}]
		},
		plugins: {
			datasource: {
				type: 'csv',
				url: '~LOG_FILE~',
				delimiter: ',',
				rowMapping: 'index',
				datasetLabels: true,
				indexLabels: true
			}
		}
	}
};


var config_without = {
	type: 'line',
	data: {
		datasets: [{
			label: 'Loaded program: ~PROGRAM_NAME~',
			tension: 0.1,
			yAxisID: 'temperature',
			backgroundColor: 'transparent',
			borderColor: chartColors.blue,
			fill: false,
			data: [~CHART_DATA~]
		}]
	},
	options: {
		legend: {
			labels: { fontColor: '#c8ccd2' }
		},
		title: {
			display: true,
			text: 'Ready to run program temperature/time graph',
			fontColor: '#e8eaed'
		},
		responsive: true,
		scales: {
			xAxes: [{
				type: 'time',
				time: {
					tooltipFormat: 'YYYY-MM-DD HH:mm',
					displayFormats: {
						millisecond: 'HH:mm:ss.SSS',
						second: 'HH:mm:ss',
						minute: 'HH:mm',
						hour: 'HH'
					}
				},
				gridLines: axisDefaults.gridLines,
				ticks: axisDefaults.ticks,
				scaleLabel: {
					display: true,
					labelString: 'Date',
					fontColor: axisDefaults.scaleLabel.fontColor
				}
			}],
			yAxes: [{
				id: 'temperature',
				gridLines: axisDefaults.gridLines,
				ticks: axisDefaults.ticks,
				scaleLabel: {
					display: true,
					labelString: 'Temperature (°C)',
					fontColor: axisDefaults.scaleLabel.fontColor
				}
			}]
		}
	}
};

var chart = new Chart(ctx, ~CONFIG~);

function chart_update(){
  chart.update();
  chart_update_id=setTimeout(chart_update, 30000);
}
