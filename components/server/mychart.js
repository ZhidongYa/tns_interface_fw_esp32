var app = {};
option = null;

var myChart = [];
var dataSet = [];
var series = [];
var dom = [];
var linesNum;
var chartsNum;
var timeIntervall;
var linesNumPerChart = 8;
var pointsNum = 100;

function addChartsElement() {
	var obj = "chartsParent";
	var parent = document.getElementById(obj);
	var div = document.createElement("div");
	div.setAttribute("name", "chart");
	parent.appendChild(div);
}

function getData(dataValue) {
    now = new Date();
    temp = now * 100;
    return {
        name: temp.toString(),
        value: [
            now,
            Math.round(dataValue)
        ]
    }
}

function dataSetInit(time) {
    time = new Date(time);
    temp = time * 100;
    return {
        name: temp.toString(),
        value: [
            time,
            0
        ]
    }
}

function myChartsInit(lines, freq) {
	disposeCharts();
    linesNum = lines;
    chartsNum = Math.ceil(linesNum / linesNumPerChart);
    timeIntervall = Math.round(1000 / freq);

    for (var i = 0; i < linesNum; i++) {
        dataSet[i] = [];
        var now = +new Date();
        for (var j = 100; j > 0; j--) {
            now = now - timeIntervall;
            dataSet[i][j] = dataSetInit(now);
        }
    }

    series = [];
    for (var i = 0; i < linesNum; i++) {
        series.push({
            name: "data" + i,
            type: 'line',
            showSymbol: false,
            hoverAnimation: false,
            data: dataSet[i]
        });
    }

    var option = [];
    for (var i = 0; i < chartsNum; i++) {
        option.push({
            title: {
                x: 'center',
                text: 'Sensor Data (Part ' + i + ')',
                textStyle: {
                    color: '#000000',
                }
            },
            legend: {
                top: 30,
            },
            xAxis: {
                type: 'time',
                splitLine: {
                    show: false
                }
            },
            yAxis: {
                type: 'value',
                boundaryGap: [0, '20%'],
                splitLine: {
                    show: false
                }
            },
            series: series.slice(i * linesNumPerChart, (i + 1) * linesNumPerChart > linesNum ? linesNum : (i + 1) * linesNumPerChart)
        });
    }
	
	for(var i = dom.length;i<chartsNum;i++)
		addChartsElement();
	
	dom = document.getElementsByName("chart");
    for (var i = 0; i < chartsNum; i++) {
        dom[i].className = "chart";
        myChart[i] = echarts.init(dom[i]);
        myChart[i].setOption(option[i], true);
    }
}

function disposeCharts(){
	for(var i=0;i<myChart.length;i++){
		myChart[i].dispose();
	}
}


function updateSeries(arrayData) {
    // if(arrayData.length!=linesNum){
    // alert("The length of the data array does not match the number of lines!");
    // return;
    // }

    for (var i = 0; i < linesNum; i++) {
        dataSet[i].shift();
        dataSet[i].push(getData(arrayData[i]));
        //dataSet[i].push(getData(Math.random() * 10));
    }

    for (var i = 0; i < chartsNum; i++) {
        myChart[i].setOption({
            series: series.slice(i * linesNumPerChart, (i + 1) * linesNumPerChart > linesNum ? linesNum : (i + 1) * linesNumPerChart)
        });
    }
}

window.onresize = function() {
    for (var i = 0; i < chartsNum; i++) {
        myChart[i].resize();
    }
};