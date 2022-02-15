var Title = "ESP Server";
document.title = "[Disconnected] " + Title;

var WEBSOCKET_CMD_READCONFIG = 82;
var WEBSOCKET_CMD_WRITECONFIG = 87;
var WEBSOCKET_CMD_START = 84;
var WEBSOCKET_CMD_STOP = 83;
var WEBSOCKET_CMD_DATA = 68;
var WEBSOCKET_CMD_ERROR = 69;
var WENSOCKET_CMD_RESET = 88;
var HEADER_SIZE = 4;
var CONFIG_LENGTH = 12;

var devAddr;
var devConfig;
var selectedDev;
var selectedDev_show;
var readDataLength;
var readDataFreq;
var isRunning = false;

initSelect('selectDev');
selectedDev = 255;
initSelect('selectDev_show');
selectedDev_show = 255;

var output = document.getElementById("output");
var websocket = new WebSocket('ws://' + location.hostname + '/');
websocket.binaryType = 'arraybuffer';

websocket.onopen = function(evt) {
    console.log('WebSocket connection opened');
    document.title = "[Connected] " + Title;
    readConfig();
}

websocket.onmessage = function(evt) {
    if (evt.data instanceof ArrayBuffer) {
        var msg = new Uint8Array(evt.data);
        if (msg[0] & 0x80) {
            alert("Lost contact with sensorsreadDataLength");
        } else if (msg[0] == WEBSOCKET_CMD_READCONFIG) {
            //初始化select元素
            initSelect('selectDev');
            selectedDev = 255;
            initSelect('selectDev_show');
            selectedDev_show = 255;
            //初始化寄存器显示列表
            deleteAllRows('tableReadConfig');
            devAddr = msg.slice(HEADER_SIZE, HEADER_SIZE + msg[2]);
            devConfig = msg.slice(HEADER_SIZE + msg[2]);
            if (devConfig.length % CONFIG_LENGTH == 0) {
                for (var i = 0; i < msg[2]; i++) {
                    addRow('tableReadConfig', devAddr[i], devConfig.slice(i * CONFIG_LENGTH, (i + 1) * CONFIG_LENGTH));
                    addOption('selectDev', i, 'devive ' + devAddr[i], false);
                    addOption('selectDev_show', i, 'devive ' + devAddr[i], false);
                }
            }
        } else if (msg[0] == WEBSOCKET_CMD_DATA && msg.length > 1) {
            if (isRunning == false) {
                if (selectedDev_show != msg[2]) {
                    selectedDev_show = msg[2];
                    set_select_checked('selectDev_show', selectedDev_show);
                }

                readDataLength = Math.ceil(msg[3] / 2);
                readDataFreq = msg[1] & 0x7F;
                myChartsInit(readDataLength, readDataFreq);
                isRunning = true;
            }
            var irq = msg[1] & 0x80;
            //output.innerHTML = irq;
            var array = msg.slice(HEADER_SIZE);
            var array16 = new Uint16Array(array.buffer);
            updateSeries(array16);
        } else if (msg[0] == WEBSOCKET_CMD_STOP) {
            setTimeout(function() {
                isRunning = false;
            }, 50);
        }
    }
}

websocket.onclose = function(evt) {
    stopStream();
    console.log('Websocket connection closed');
    document.title = "[Closed] " + Title;
}

websocket.onerror = function(evt) {
    console.log('Websocket error: ' + evt);
    document.title = "[Error] " + Title;
}

function readConfig() {
    if (isRunning == true)
        stopStream();
    var msg = new Uint8Array(1);
    msg[0] = WEBSOCKET_CMD_READCONFIG;
    websocket.send(msg);
}

function writeConfig() {
    if (isRunning == true)
        stopStream();

    var config = document.getElementById('tableSetConfig').getElementsByTagName("input");
    var msg = new Uint8Array(config.length + 2);
    msg[0] = WEBSOCKET_CMD_WRITECONFIG;
    msg[1] = selectedDev;
    for (var i = 0; i < config.length; i++)
        msg[i + 2] = config[i].value;
    websocket.send(msg);
    readConfig();
}

function resetAllDevices() {
    if (isRunning == true)
        stopStream();
    var msg = new Uint8Array(1);
    msg[0] = WENSOCKET_CMD_RESET;
    websocket.send(msg);

    setTimeout(function() {
        readConfig();
    }, 50);
}

function getStart() {
    if (isRunning == true)
        return;
    var msg = new Uint8Array(2);
    msg[0] = WEBSOCKET_CMD_START;
    msg[1] = selectedDev_show;
    websocket.send(msg);
}

function stopStream() {
    if (isRunning == false)
        return;
    var msg = new Uint8Array(1);
    msg[0] = WEBSOCKET_CMD_STOP;
    websocket.send(msg);
    setTimeout(function() {
        isRunning = false;
    }, 50);
}

function saveData() {
    //set_select_checked('selectDev_show', 0xff)
    //myChartsInit(12, readDataFreq);
    //var bodyObj = document.getElementById('tableSetConfig');
    //bodyObj.rows[1].cells[0].innerHTML = 'dads';
}

/**
 * tbody元素动态添加行
 *selectId
 * @param tbodyID
 * @param device 传感器地址
 * @param config 传感器寄存器配置信息
 */
function addRow(tbodyID, device, config) {
    var bodyObj = document.getElementById(tbodyID);
    if (bodyObj == null) {
        alert("Body of Table not Exist!");
        return;
    }
    var rowCount = bodyObj.rows.length;
    var cellCount = bodyObj.rows[0].cells.length;
    var newRow = bodyObj.insertRow(rowCount++);
    if (rowCount % 2 == 0)
        newRow.style.backgroundColor = "#D3D3D3";
    newRow.insertCell(0).innerHTML = device;
    for (var i = 1; i < cellCount; i++)
        newRow.insertCell(i).innerHTML = config[i - 1];
}

/**
 * tbody元素删除所有行
 *
 * @param tbodyID
 */
function deleteAllRows(tbodyID) {
    var bodyObj = document.getElementById(tbodyID);
    if (bodyObj == null) {
        alert("Body of Table not Exist!");
        return;
    }
    var rowCount = bodyObj.rows.length;
    for (var i = rowCount - 1; i > 0; i--)
        bodyObj.deleteRow(i);
}

/** 
 * select元素动态添加option 
 *  
 * @param selectEleId select元素id 
 * @param width 要添加的option的value属性值 
 * @param height 要添加的option的innerHTML 
 * @param selected 是否选中 
 */
function addOption(selectEleId, optionValue, optionInnerHTML, selected) {
    var selectEle = document.getElementById(selectEleId);
    var optionObj = document.createElement("option");
    optionObj.value = optionValue;
    optionObj.innerHTML = optionInnerHTML;
    optionObj.selected = selected;
    selectEle.appendChild(optionObj);
}

/** 
 * 动态删除select元素中所有的option 
 *  
 * @param selectEleId select元素id 
 */
function deleteAllOptions(selectEleId) {
    document.getElementById(selectEleId).options.length = 0;
}

/** 
 * 初始化select元素
 *  
 * @param selectEleId select元素id 
 */
function initSelect(selectEleId) {
    deleteAllOptions(selectEleId);
    addOption(selectEleId, 255, "All Selected", true);
}

/** 
 * select onchange回调函数
 */
function selectDevOnChange(obj) {
    var configInput = document.getElementById('tableSetConfig').getElementsByTagName("input");
    var config;
    var i;

    var value = obj.options[obj.selectedIndex].value
    if (value == 255) {
        selectedDev = value
    } else {
        selectedDev = devAddr[value];
        config = devConfig.slice(value * CONFIG_LENGTH, (value + 1) * CONFIG_LENGTH)
        for (i = 0; i < configInput.length - 1; i++)
            configInput[i].value = config[i];
        configInput[i].value = 128;
    }
    //alert(value);
}


/** 
 * select onchange回调函数
 */
function selectDevShowOnChange(obj) {
    stopStream();
    selectedDev_show = obj.options[obj.selectedIndex].value;
    //alert(selectedDev_show);
}


/** 
 * 设置select控件选中 
 * @param obj select的id值 
 * @param checkValue 选中option的值 
 */
function set_select_checked(obj, checkValue) {
    var select = document.getElementById(obj);

    for (var i = 0; i < select.options.length; i++) {
        if (select.options[i].value == checkValue) {
            select.options[i].selected = true;
            break;
        }
    }
}