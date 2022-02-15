#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "lwip/api.h"

#include "string.h"

#include "esp_sensor.h"

#include "webserver.h"
#include "websocket_server.h"

static QueueHandle_t client_queue;
const static int client_queue_size = 5;

//sensor
extern SemaphoreHandle_t xSemaphore;
extern uint8_t sensor_table[];
extern uint8_t sensor_table_length;
extern uint8_t SENSOR_CONFIGS_LENGTH;
uint8_t sensor_selected;
bool sensor_task_flag = false;

void sensor_task(void *pvParameters)
{
    const static char *TAG = "sensor_task";

    user_header_t header;
    header.param.bit.CMD = WEBSOCKET_CMD_DATA;
    header.param.bit.ERROR = 0;
    header.param.bit.FREQ = CONFIG_READ_FREQUENCY;
    //header.state.sensor.DEVS = sensor_table_length;
    //header.state.sensor.LEN = CONFIG_READ_SENSOR_DATA_LENGTH;
    header.state.sensor.DEVS = sensor_selected;
    if(sensor_selected == 0xff)
        header.state.sensor.LEN = sensor_table_length*CONFIG_READ_SENSOR_DATA_LENGTH;
    else
        header.state.sensor.LEN = CONFIG_READ_SENSOR_DATA_LENGTH;


    uint8_t header_size = sizeof(header);
    uint8_t data[header.state.sensor.LEN];
    uint8_t out[header_size + header.state.sensor.LEN];

    esp_err_t ret = ESP_OK;
    while (sensor_task_flag)
    {
        if (xSemaphoreTake(xSemaphore, 10 / portTICK_RATE_MS) == pdTRUE)
        {
            header.param.bit.IRQ = get_sensor_irq_state();
            ret = read_sensor_data(sensor_selected, 0,data, CONFIG_READ_SENSOR_DATA_LENGTH);
            if (ret != ESP_OK)
            {
                header.param.bit.ERROR = 1;
                header.param.bit.ERROR = 1;
                /* you can add some details about the error */
                //header.state.err.ERROR_ADDR = 0xFF;
                //header.state.err.ERROR_MSG = ;
                memcpy(out,&header,header_size);
                ws_server_send_binary_all_from_callback(out,header_size);
                ESP_LOGE(TAG, "Can't read data from sensors");
            }
            else
            {
                ESP_LOGI(TAG, "send %d bytes sensor data",sizeof(out));
                memcpy(out, &header, header_size);
                memcpy(out+header_size,&data,sizeof(data));
                ws_server_send_binary_all_from_callback(out, sizeof(out));
            }
        }
    }
    ESP_LOGI(TAG, "Sensor Task deleted");
    vTaskDelete(NULL);
}

//handles message
static void msg_bin_handle(char *msg, uint64_t len)
{
    if (len == 0)
        return;

    const static char *TAG = "msg_handle";
    user_header_t header;
    header.param.bit.FREQ = CONFIG_READ_FREQUENCY;
    header.param.bit.IRQ = 0;
    header.state.sensor.DEVS = sensor_table_length;
    header.state.sensor.LEN  = CONFIG_READ_SENSOR_DATA_LENGTH;
    uint8_t header_size = sizeof(header);
    
    uint8_t configsLength = SENSOR_CONFIGS_LENGTH;
    uint8_t *configs;
    uint8_t *data;
    uint8_t *out;

    switch (msg[0])
    {
    case WEBSOCKET_CMD_WRITECONFIG:
        if (len != configsLength + 2)
        {
            header.param.bit.CMD = WEBSOCKET_CMD_WRITECONFIG;
            header.param.bit.ERROR = 1;
            /* you can add some details about the error */
            //header.state.err.ERROR_ADDR = 0xFF;
            //header.state.err.ERROR_MSG = ;
            out = malloc(header_size);
            out = memcpy(out,&header,header_size);
            ws_server_send_binary_all_from_callback(out,header_size);
            free(out);
            return;
        }

        //  msg[0] | msg[1] | msg[...]
        //   cmd   |  addr  | configs
        configs = malloc(configsLength);
        memcpy(configs, msg + 2, configsLength);
        //configure all sensors
        if (msg[1] == 0xff)
        {
            for (int i = 0; i < sensor_table_length; i++)
            {
                if (set_sensor_configs(sensor_table[i], configs, configsLength) != ESP_OK)
                {
                    ESP_LOGE(TAG, "Sensor at the address %x inited not correct!", sensor_table[i]);
                    header.param.bit.CMD = WEBSOCKET_CMD_WRITECONFIG;
                    header.param.bit.ERROR = 1;
                    /* you can add some details about the error */
                    //header.state.err.ERROR_ADDR = 0xFF;
                    //header.state.err.ERROR_MSG = ;
                    out = malloc(header_size);
                    out = memcpy(out,&header,header_size);
                    ws_server_send_binary_all_from_callback(out,header_size);
                    free(out);
                }
            }
        }
        //configure given sensor
        else
        {
            if (set_sensor_configs(msg[1], configs, configsLength) != ESP_OK)
            {
                ESP_LOGE(TAG, "Sensor at the address %x inited not correct!", msg[1]);
                header.param.bit.CMD = WEBSOCKET_CMD_WRITECONFIG;
                header.param.bit.ERROR = 1;
                /* you can add some details about the error */
                //header.state.err.ERROR_ADDR = mag[1];
                //header.state.err.ERROR_MSG = ;
                out = malloc(header_size);
                out = memcpy(out,&header,header_size);
                ws_server_send_binary_all_from_callback(out,header_size);
                free(out);
            }
        }
        free(configs);
        break;

    case WEBSOCKET_CMD_READCONFIG:
        header.param.bit.CMD = WEBSOCKET_CMD_READCONFIG;
        header.param.bit.ERROR = 0;
        configs = malloc(configsLength);

        uint16_t dataLength = sensor_table_length + sensor_table_length * configsLength;
        data = malloc(dataLength);
        memcpy(data, &sensor_table, sensor_table_length);
        for (uint8_t i = 0; i < sensor_table_length; i++)
        {
            if (read_sensor_configs(sensor_table[i], configs) == ESP_FAIL)
                memset(configs, -1, configsLength);
            memcpy(data + sensor_table_length + i * configsLength, configs, configsLength);
        }
        out = malloc(header_size + dataLength);
        memcpy(out, &header,header_size);
        memcpy(out + header_size, data, dataLength);
        ws_server_send_binary_all_from_callback(out, header_size + dataLength);
        free(configs);
        free(data);
        free(out);
        break;

    case WEBSOCKET_CMD_START:
        sensor_task_flag = true;
        sensor_selected = msg[1];
        xTaskCreate(&sensor_task, "sensortask", 4000, NULL, 5, NULL);
        ESP_LOGI(TAG, "Sensor Task created");
        break;

    case WEBSOCKET_CMD_STOP:
        sensor_task_flag = false;
        header.param.bit.CMD = WEBSOCKET_CMD_STOP;
        header.param.bit.ERROR = 0;
        out = malloc(header_size);
        out = memcpy(out,&header,header_size);
        ws_server_send_binary_all_from_callback(out,header_size);
        free(out);
        break;

    case WENSOCKET_CMD_RESET:
        sensor_task_flag = false;
        vTaskDelay(200/portTICK_RATE_MS);
        reset_sensor();
        header.param.bit.CMD = WENSOCKET_CMD_RESET;
        header.param.bit.ERROR = 0;
        out = malloc(header_size);
        out = memcpy(out,&header,header_size);
        ws_server_send_binary_all_from_callback(out,header_size);
        free(out);
        break;

    default:
        ESP_LOGE(TAG, "error on read, undefined command: %d", msg[0]);
        break;
    }
}

// handles websocket events
void websocket_callback(uint8_t num, WEBSOCKET_TYPE_t type, char *msg, uint64_t len)
{
    const static char *TAG = "websocket_callback";

    switch (type)
    {
    case WEBSOCKET_CONNECT:
        ESP_LOGI(TAG, "client %i connected!", num);
        break;
    case WEBSOCKET_DISCONNECT_EXTERNAL:
        sensor_task_flag = false;
        break;
    case WEBSOCKET_DISCONNECT_INTERNAL:
        ESP_LOGI(TAG, "client %i was disconnected", num);
        break;
    case WEBSOCKET_DISCONNECT_ERROR:
        ESP_LOGI(TAG, "client %i was disconnected due to an error", num);
        //do something
        break;
    case WEBSOCKET_TEXT:
        ESP_LOGI(TAG, "client %i received message of size %i:\n%s", num, (uint32_t)len, msg);
        break;
    case WEBSOCKET_BIN:
        ESP_LOGI(TAG, "client %i sent binary message of size %i:\n%s", num, (uint32_t)len, msg);
        msg_bin_handle(msg, len);
        break;
    case WEBSOCKET_PING:
        ESP_LOGI(TAG, "client %i pinged us with message of size %i:\n%s", num, (uint32_t)len, msg);
        break;
    case WEBSOCKET_PONG:
        ESP_LOGI(TAG, "client %i responded to the ping", num);
        break;
    }
}

// serves any clients
static void http_serve(struct netconn *conn)
{
    const static char *TAG = "http_server";
    const static char HTML_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
    const static char ERROR_HEADER[] = "HTTP/1.1 404 Not Found\nContent-type: text/html\n\n";
    const static char JS_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/javascript\n\n";
    const static char CSS_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/css\n\n";
    //const static char PNG_HEADER[] = "HTTP/1.1 200 OK\nContent-type: image/png\n\n";
    const static char ICO_HEADER[] = "HTTP/1.1 200 OK\nContent-type: image/x-icon\n\n";
    //const static char PDF_HEADER[] = "HTTP/1.1 200 OK\nContent-type: application/pdf\n\n";
    //const static char EVENT_HEADER[] = "HTTP/1.1 200 OK\nContent-Type: text/event-stream\nCache-Control: no-cache\nretry: 3000\n\n";
    struct netbuf *inbuf;
    static char *buf;
    static uint16_t buflen;
    static err_t err;

    // default page
    extern const uint8_t root_html_start[] asm("_binary_root_html_start");
    extern const uint8_t root_html_end[] asm("_binary_root_html_end");
    const uint32_t root_html_len = root_html_end - root_html_start;

    // websocket.js
    extern const uint8_t websocket_js_start[] asm("_binary_websocket_js_start");
    extern const uint8_t websocket_js_end[] asm("_binary_websocket_js_end");
    const uint32_t websocket_js_len = websocket_js_end - websocket_js_start;

    // mychart.js
    extern const uint8_t mychart_js_start[] asm("_binary_mychart_js_start");
    extern const uint8_t mychart_js_end[] asm("_binary_mychart_js_end");
    const uint32_t mychart_js_len = mychart_js_end - mychart_js_start;

    // echarts.min.js
    extern const uint8_t echarts_js_start[] asm("_binary_echarts_min_js_start");
    extern const uint8_t echarts_js_end[] asm("_binary_echarts_min_js_end");
    const uint32_t echarts_js_len = echarts_js_end - echarts_js_start;

    // style.css
    extern const uint8_t style_css_start[] asm("_binary_style_css_start");
    extern const uint8_t style_css_end[] asm("_binary_style_css_end");
    const uint32_t style_css_len = style_css_end - style_css_start;

    // ipr.ico
    extern const uint8_t ipr_ico_start[] asm("_binary_ipr_ico_start");
    extern const uint8_t ipr_ico_end[] asm("_binary_ipr_ico_end");
    const uint32_t ipr_ico_len = ipr_ico_end - ipr_ico_start;

    // error page
    extern const uint8_t error_html_start[] asm("_binary_error_html_start");
    extern const uint8_t error_html_end[] asm("_binary_error_html_end");
    const uint32_t error_html_len = error_html_end - error_html_start;

    netconn_set_recvtimeout(conn, 1000); // allow a connection timeout of 1 second
    ESP_LOGI(TAG, "reading from client...");
    err = netconn_recv(conn, &inbuf);
    ESP_LOGI(TAG, "read from client");
    if (err == ERR_OK)
    {
        netbuf_data(inbuf, (void **)&buf, &buflen);
        if (buf)
        {

            // default page
            if (strstr(buf, "GET / ") && !strstr(buf, "Upgrade: websocket"))
            {
                ESP_LOGI(TAG, "Sending /");
                netconn_write(conn, HTML_HEADER, sizeof(HTML_HEADER) - 1, NETCONN_NOCOPY);
                netconn_write(conn, root_html_start, root_html_len, NETCONN_NOCOPY);
                netconn_close(conn);
                netconn_delete(conn);
                netbuf_delete(inbuf);
            }

            // default page websocket
            else if (strstr(buf, "GET / ") && strstr(buf, "Upgrade: websocket"))
            {
                ESP_LOGI(TAG, "Requesting websocket on /");
                ws_server_add_client(conn, buf, buflen, "/", websocket_callback);
                netbuf_delete(inbuf);
            }

            else if (strstr(buf, "GET /websocket.js "))
            {
                ESP_LOGI(TAG, "Sending /websocket.js");
                netconn_write(conn, JS_HEADER, sizeof(JS_HEADER) - 1, NETCONN_NOCOPY);
                netconn_write(conn, websocket_js_start, websocket_js_len, NETCONN_NOCOPY);
                netconn_close(conn);
                netconn_delete(conn);
                netbuf_delete(inbuf);
            }

            else if (strstr(buf, "GET /mychart.js "))
            {
                ESP_LOGI(TAG, "Sending /mychart.js");
                netconn_write(conn, JS_HEADER, sizeof(JS_HEADER) - 1, NETCONN_NOCOPY);
                netconn_write(conn, mychart_js_start, mychart_js_len, NETCONN_NOCOPY);
                netconn_close(conn);
                netconn_delete(conn);
                netbuf_delete(inbuf);
            }

            else if (strstr(buf, "GET /echarts.min.js "))
            {
                ESP_LOGI(TAG, "Sending /echarts.min.js");
                netconn_write(conn, JS_HEADER, sizeof(JS_HEADER) - 1, NETCONN_NOCOPY);
                netconn_write(conn, echarts_js_start, echarts_js_len, NETCONN_NOCOPY);
                netconn_close(conn);
                netconn_delete(conn);
                netbuf_delete(inbuf);
            }

            else if (strstr(buf, "GET /style.css "))
            {
                ESP_LOGI(TAG, "Sending /style.css");
                netconn_write(conn, CSS_HEADER, sizeof(CSS_HEADER) - 1, NETCONN_NOCOPY);
                netconn_write(conn, style_css_start, style_css_len, NETCONN_NOCOPY);
                netconn_close(conn);
                netconn_delete(conn);
                netbuf_delete(inbuf);
            }

            else if (strstr(buf, "GET /ipr.ico "))
            {
                ESP_LOGI(TAG, "Sending ipr.ico");
                netconn_write(conn, ICO_HEADER, sizeof(ICO_HEADER) - 1, NETCONN_NOCOPY);
                netconn_write(conn, ipr_ico_start, ipr_ico_len, NETCONN_NOCOPY);
                netconn_close(conn);
                netconn_delete(conn);
                netbuf_delete(inbuf);
            }

            else if (strstr(buf, "GET /"))
            {
                ESP_LOGI(TAG, "Unknown request, sending error page: %s", buf);
                netconn_write(conn, ERROR_HEADER, sizeof(ERROR_HEADER) - 1, NETCONN_NOCOPY);
                netconn_write(conn, error_html_start, error_html_len, NETCONN_NOCOPY);
                netconn_close(conn);
                netconn_delete(conn);
                netbuf_delete(inbuf);
            }

            else
            {
                ESP_LOGI(TAG, "Unknown request");
                netconn_close(conn);
                netconn_delete(conn);
                netbuf_delete(inbuf);
            }
        }
        else
        {
            ESP_LOGI(TAG, "Unknown request (empty?...)");
            netconn_close(conn);
            netconn_delete(conn);
            netbuf_delete(inbuf);
        }
    }
    else
    { // if err==ERR_OK
        ESP_LOGI(TAG, "error on read, closing connection");
        netconn_close(conn);
        netconn_delete(conn);
        netbuf_delete(inbuf);
    }
}

// handles clients when they first connect. passes to a queue
void server_task(void *pvParameters)
{
    const static char *TAG = "server_task";
    struct netconn *conn, *newconn;
    static err_t err;
    client_queue = xQueueCreate(client_queue_size, sizeof(struct netconn *));

    conn = netconn_new(NETCONN_TCP);
    netconn_bind(conn, NULL, 80);
    netconn_listen(conn);
    ESP_LOGI(TAG, "server listening");
    do
    {
        err = netconn_accept(conn, &newconn);
        ESP_LOGI(TAG, "new client");
        if (err == ERR_OK)
        {
            xQueueSendToBack(client_queue, &newconn, portMAX_DELAY);
            //http_serve(newconn);
        }
    } while (err == ERR_OK);
    netconn_close(conn);
    netconn_delete(conn);
    ESP_LOGE(TAG, "task ending, rebooting board");
    esp_restart();
}

// receives clients from queue, handles them
void server_handle_task(void *pvParameters)
{
    const static char *TAG = "server_handle_task";
    struct netconn *conn;
    ESP_LOGI(TAG, "task starting");
    for (;;)
    {
        xQueueReceive(client_queue, &conn, portMAX_DELAY);
        if (!conn)
            continue;
        http_serve(conn);
    }
    vTaskDelete(NULL);
}
