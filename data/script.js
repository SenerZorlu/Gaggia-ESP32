// Get current sensor readings when the page loads  
window.addEventListener('load', getReadings);

let pointer = document.getElementById('e-pointer');
let timer = document.getElementById("time");
let svgheater = document.getElementById("svg-heater");
let coffeeIcon = document.getElementById("coffee_icon");
let settingsIcon = document.getElementById("settings");
let temp_card = document.getElementById("temp_card");
let chart_card = document.getElementById("chart_card");


// Create Humidity Gauge
var gaugeTemp = new RadialGauge({
  renderTo: 'gauge-temperature',
  width: 300,
  height: 300,
  units: "°C",
  minValue: 0,
  maxValue: 120,
  colorValueBoxRect: "#049faa",
  colorValueBoxRectEnd: "#049faa",
  colorValueBoxBackground: "#f1fbfc",
  valueInt: 3,
  valueDec: 0,
  majorTicks: [
    "0",
    "20",
    "40",
    "60",
    "80",
    "100",
    "120"

  ],
  minorTicks: 4,
  strokeTicks: true,
  highlights: [
    {
      "from": 85,
      "to": 95,
      "color": "#03C0C1"
    },
    {

      "from": 95,
      "to": 120,
      "color": "#F7958E"
    }
  ],
  colorPlate: "#fff",
  colorStrokeTicks: "#B6B6B6",
  borderShadowWidth: 0,
  borders: false,
  needleType: "line",
  colorNeedle: "#F7958E",
  colorNeedleEnd: "#F7958E",
  colorValueBoxBackground: "#B6B6B6",
  needleWidth: 2,
  needleCircleSize: 3,
  colorNeedleCircleOuter: "#007F80",
  needleCircleOuter: true,
  needleCircleInner: false,
  animationDuration: 400,
  animationRule: "linear"
}).draw();

var chartT = new Highcharts.Chart({
  chart: {
    renderTo: 'chart-temperature',
    height: 300
  },
  series: [
    {
      name: 'Temperature',
      type: 'line',
      color: '#F7958E',
      marker: {
        symbol: 'circle',
        radius: 3,
        fillColor: '#F7958E',
      }
    }
  ],
  title: {
    text: undefined
  },
  xAxis: {
    title: {
      text: 'Brew Time'
    }
  },
  yAxis: {
    title: {
      text: 'Temperature Celsius Degrees'
    }
  },
  credits: {
    enabled: false
  }
});

//Plot temperature in the temperature chart
var last_x;
function plotTemperature(temp, time) {




  var x = parseInt(time);

  if (x != last_x) {
    last_x = x;
    var y = parseInt(temp);

    if (chartT.series[0].data.length > 160) {
      chartT.series[0].addPoint([x, y], true, true, true);
    } else {
      console.log(time);
      chartT.series[0].addPoint([x, y], true, false, true);
    }
  }


}

// Function to get current readings on the webpage when it loads for the first time
function getReadings() {
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function () {
    if (this.readyState == 4 && this.status == 200) {
      var myObj = JSON.parse(this.responseText);
      console.log(myObj);
      var temp = myObj.temperature;
      gaugeTemp.value = temp;
    }
  };
  xhr.open("GET", "/readings", true);
  xhr.send();
}

if (!!window.EventSource) {
  var source = new EventSource('/events');

  source.addEventListener('open', function (e) {
    console.log("Events Connected");
  }, false);

  source.addEventListener('error', function (e) {
    if (e.target.readyState != EventSource.OPEN) {
      console.log("Events Disconnected");
    }
  }, false);

  source.addEventListener('message', function (e) {
    console.log("message", e.data);
  }, false);

  source.addEventListener('new_readings', function (e) {
    console.log("new_readings", e.data);
    var myObj = JSON.parse(e.data);
    console.log(myObj);
    gaugeTemp.value = myObj.temperature;

    if (myObj.time != "0") {
      timer.innerHTML = myObj.time_label;
      coffeeIcon.style.display = "block";
      settingsIcon.style.display = "none";
      temp_card.style.display = "none";
      chart_card.style.display = "block";
      chartT.reflow();
      var angle = (360 * (60 - parseInt(myObj.time)) / 60) - 360;
      pointer.style.transform = `rotate(${angle}deg)`;
      plotTemperature(myObj.temperature, myObj.time);
    } else {

      timer.innerHTML = "0";
      //clear chart series
      for (let i = 0; i < chartT.series[0].data.length; i++) {
        chartT.series[0].removePoint(i, true);
      };
      pointer.style.transform = `rotate(0deg)`;
      coffeeIcon.style.display = "none";
      settingsIcon.style.display = "block";
      temp_card.style.display = "block";
      chart_card.style.display = "none";

    }
    // if (myObj.heaterstate != "1") {
    //   svgheater.style.fill = '#F8F8FF';
    // } else {
    //   svgheater.style.fill = '#F7958E';
    // }

  }, false);
}