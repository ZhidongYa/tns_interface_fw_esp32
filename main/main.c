#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include <string.h>
#include <sys/param.h>
#include <stdlib.h>


#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_event.h"


#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "addr_from_stdin.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>


//sensor
#include "esp_sensor.h"

//timer
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"



//webserver
#include "webserver.h"
#include "websocket_server.h"
//#include "tcp_client.h"

#if defined(CONFIG_EXAMPLE_IPV4)
#define HOST_IP_ADDR CONFIG_EXAMPLE_IPV4_ADDR
#elif defined(CONFIG_EXAMPLE_IPV6)
#define HOST_IP_ADDR CONFIG_EXAMPLE_IPV6_ADDR
#else
#define HOST_IP_ADDR ""
#endif
#define PORT CONFIG_EXAMPLE_PORT

#define TAG  "olimex-project"

#define TIMER_DIVIDER 16                             /*!<  Hardware timer clock divider*/
#define TIMER_SCALE (TIMER_BASE_CLK / TIMER_DIVIDER) /*!< convert counter value to seconds*/
#define TIMER_INDEX TIMER_0
#define TIMER_FREQ  CONFIG_READ_FREQUENCY
SemaphoreHandle_t xSemaphore;
extern uint8_t sensor_table_length;
extern uint8_t sensor_table[];
extern uint8_t I2C_NUM;
extern uint8_t SENSOR_CONFIGS_LENGTH;

/**
 * transfer char configrations to a char array, str is char configrations, ch is separator, args is the array, size is the size of array
 */
void split(char *str,const char *ch,char ***args,int *size){
 	int i=0,word=0,sum=0;
 	for(;str[i]!='\0';i++){
 		if(str[i]==ch[0])
 		word=0;
 		else if(word==0){
 			word=1;
 			sum++;
		 }
	 }
	 if(sum==0)
	 return;
	 *size=sum;
	 *args=(char**)malloc(sizeof(char*)*sum);
	 char *s;
	 s=(char*)strtok(str,ch);
	 i=0;
	 while(s!=NULL){
	 	(*args)[i]=s;
	 	s=(char*)strtok(NULL,ch);
	 	i++;
	 }

 }


/**
 * @brief Timer group0 ISR handler
 */
void IRAM_ATTR timer_group0_isr(void *para)
{
    int timer_idx = (int)para;

    /* Retrieve the interrupt status and the counter value
       from the timer that reported the interrupt */
    uint32_t intr_status = TIMERG0.int_st_timers.val;
    TIMERG0.hw_timer[timer_idx].update = 1;

    /* Clear the interrupt
       and update the alarm time for the timer with without reload */
    if ((intr_status & BIT(timer_idx)) && timer_idx == TIMER_0)
    {
        TIMERG0.int_clr_timers.t0 = 1;
    }
    else if ((intr_status & BIT(timer_idx)) && timer_idx == TIMER_1)
    {
        TIMERG0.int_clr_timers.t1 = 1;
    }

    /* After the alarm has been triggered
      we need enable it again, so it is triggered the next time */
    TIMERG0.hw_timer[timer_idx].config.alarm_en = TIMER_ALARM_EN;

    /* Now just send the event data back to the main program task */
    xSemaphoreGiveFromISR(xSemaphore, NULL);
}

 void tcp_client_task(void *pvParameters)
{
    char rx_buffer[128];
    char sx_buffer[100]={'\0'};
    char host_ip[] = HOST_IP_ADDR;
    int addr_family = 0;
    int ip_protocol = 0;
    uint8_t *configs;
    

    while (1) {
#if defined(CONFIG_EXAMPLE_IPV4)
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(host_ip);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
#elif defined(CONFIG_EXAMPLE_IPV6)
        struct sockaddr_in6 dest_addr = { 0 };
        inet6_aton(host_ip, &dest_addr.sin6_addr);
        dest_addr.sin6_family = AF_INET6;
        dest_addr.sin6_port = htons(PORT);
        dest_addr.sin6_scope_id = esp_netif_get_netif_impl_index(EXAMPLE_INTERFACE);
        addr_family = AF_INET6;
        ip_protocol = IPPROTO_IPV6;
#elif defined(CONFIG_EXAMPLE_SOCKET_IP_INPUT_STDIN)
        struct sockaddr_storage dest_addr = { 0 };
        ESP_ERROR_CHECK(get_addr_from_stdin(PORT, SOCK_STREAM, &ip_protocol, &addr_family, &dest_addr));
#endif
        int sock =  socket(addr_family, SOCK_STREAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created");
        vTaskDelay(4000 / portTICK_PERIOD_MS);//4 second
        ESP_LOGI(TAG, "connecting to %s:%d", host_ip, PORT);
        int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in6));
        if (err != 0) {
            ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Successfully connected");

        while (1) {
            uint8_t data[CONFIG_READ_SENSOR_DATA_LENGTH];        
            for (uint8_t sensor_selected = 0; sensor_selected < sensor_table_length; sensor_selected++)//read all sensor data
            {
               
            if (read_sensor_data(sensor_selected, 0, data, CONFIG_READ_SENSOR_DATA_LENGTH)== ESP_OK)
            {   
                //ESP_LOGE(TAG,"sensor_data: %d %d %d %d %d %d %d %d %d %d %d %d", data[0],data[1],data[2],data[3],data[22], data[23], data[24], data[25], data[26], data[27], data[28], data[29]);
                uint16_t out[33]; //(CONFIG_READ_SENSOR_DATA_LENGTH+1)/2
                merge_data(data, out, CONFIG_READ_SENSOR_DATA_LENGTH);
                strcat(sx_buffer,"Sensor address");
                char address[12];
                sprintf(address,"%d",sensor_selected);
			    strcat(sx_buffer,address);
                strcat(sx_buffer,":");
                for(int i=0;i<33;i++)
	           {
                char temp[10];
                sprintf(temp,"%d",out[i]);
			    strcat(sx_buffer,temp);
                strcat(sx_buffer," ");
               }
	            //ESP_LOGE(TAG,"sx_buffer: %s", sx_buffer);
                int err = send(sock, sx_buffer, strlen(sx_buffer), 0);   
            if (err < 0) {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                break;
                         }
                memset(sx_buffer, 0, sizeof(sx_buffer));
            }
            
            }
            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            // Error occurred during receiving
             if (len < 0) {
                ESP_LOGE(TAG, "recv failed: errno %d", errno);
                break;
            } 
            // Data received
            else
        {
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, host_ip);
                ESP_LOGI(TAG, "%s", rx_buffer);
                configs=malloc(SENSOR_CONFIGS_LENGTH);
                char **args;
                int size;
                split(rx_buffer,",",&args,&size);
	            for(int i=0;i<SENSOR_CONFIGS_LENGTH;i++)
	            {configs[i]=atoi(args[i+1]);}
	             if (atoi(args[0]) == 0xff)// config alle sensor
                {   
                    for (int i = 0; i < sensor_table_length; i++)
            {
                if (set_sensor_configs(sensor_table[i], configs, SENSOR_CONFIGS_LENGTH) != ESP_OK)
                {
                    ESP_LOGE(TAG, "Sensor at the address %x inited not correct!", sensor_table[i]);
                }  
            }
                }
                else{
                    if (set_sensor_configs(atoi(args[0]), configs, SENSOR_CONFIGS_LENGTH) != ESP_OK)// nur config ein sensor
                {
                    ESP_LOGE(TAG, "Sensor at the address %x inited not correct!", atoi(args[0]));
                } 
                    } 
            free(configs);
            free(args);
        }
          
           vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
                        } 
         }
    vTaskDelete(NULL);
   
}

void udp_client_task(void *pvParameters)
{
    char rx_buffer[128];
    char sx_buffer[100]={'\0'};
    uint8_t *configs;
    char host_ip[] = HOST_IP_ADDR;
    int addr_family = 0;
    int ip_protocol = 0;

    while (1) {

#if defined(CONFIG_EXAMPLE_IPV4)
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
#elif defined(CONFIG_EXAMPLE_IPV6)
        struct sockaddr_in6 dest_addr = { 0 };
        inet6_aton(HOST_IP_ADDR, &dest_addr.sin6_addr);
        dest_addr.sin6_family = AF_INET6;
        dest_addr.sin6_port = htons(PORT);
        dest_addr.sin6_scope_id = esp_netif_get_netif_impl_index(EXAMPLE_INTERFACE);
        addr_family = AF_INET6;
        ip_protocol = IPPROTO_IPV6;
#elif defined(CONFIG_EXAMPLE_SOCKET_IP_INPUT_STDIN)
        struct sockaddr_storage dest_addr = { 0 };
        ESP_ERROR_CHECK(get_addr_from_stdin(PORT, SOCK_DGRAM, &ip_protocol, &addr_family, &dest_addr));
#endif

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created, sending to %s:%d", HOST_IP_ADDR, PORT);

        while (1) {
            
            uint8_t data[CONFIG_READ_SENSOR_DATA_LENGTH];
            //sensor_table_length = find_sensor_devices();
            for (uint8_t sensor_selected = 0; sensor_selected < sensor_table_length; sensor_selected++)//read all sensor data
            {
               
            if (read_sensor_data(sensor_selected, 0, data, CONFIG_READ_SENSOR_DATA_LENGTH)== ESP_OK)
            {   
                //ESP_LOGE(TAG,"sensor_data: %d %d %d %d %d %d %d %d %d %d %d %d", data[0],data[1],data[2],data[3],data[22], data[23], data[24], data[25], data[26], data[27], data[28], data[29]);
                uint16_t out[33]; //(CONFIG_READ_SENSOR_DATA_LENGTH+1)/2
                merge_data(data, out, CONFIG_READ_SENSOR_DATA_LENGTH);
                strcat(sx_buffer,"Sensor address");
                char address[12];
                sprintf(address,"%d",sensor_selected);
			    strcat(sx_buffer,address);
                strcat(sx_buffer,":");
                for(int i=0;i<33;i++)
	           {
                char temp[10];
                sprintf(temp,"%d",out[i]);
			    strcat(sx_buffer,temp);
                strcat(sx_buffer," ");
               }
	            //ESP_LOGE(TAG,"sx_buffer: %s", sx_buffer);
                
            int err = sendto(sock, sx_buffer, strlen(sx_buffer), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            if (err < 0) {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                break;
            }
             memset(sx_buffer, 0, sizeof(sx_buffer));            
            }           
            }

            struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            // Error occurred during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, host_ip);
                ESP_LOGI(TAG, "%s", rx_buffer);
                if (strncmp(rx_buffer, "OK: ", 4) == 0) {
                    ESP_LOGI(TAG, "Received expected message, reconnecting");
                    break;
                }
                configs=malloc(SENSOR_CONFIGS_LENGTH);
                char **args;
                int size;
                split(rx_buffer,",",&args,&size);
	            for(int i=0;i<SENSOR_CONFIGS_LENGTH;i++)
	            {configs[i]=atoi(args[i+1]);
	             }
                 if (atoi(args[0]) == 0xff)
                 { 
                   for (int i = 0; i < sensor_table_length; i++)
            {
                if (set_sensor_configs(sensor_table[i], configs, SENSOR_CONFIGS_LENGTH) != ESP_OK)
                {
                    ESP_LOGE(TAG, "Sensor at the address %x inited not correct!", sensor_table[i]);
                }  
            }  
                 }
                 else{
                     
                    if (set_sensor_configs(atoi(args[0]), configs, SENSOR_CONFIGS_LENGTH) != ESP_OK)
                {
                    ESP_LOGE(TAG, "Sensor at the address %x inited not correct!", atoi(args[0]));
                } 
                    } 
            free(args);
            free(configs);
            }

            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
} 


 

/*
 * Initialize selected timer of the timer group 0
 *
 * timer_idx - the timer number to initialize
 * auto_reload - should the timer auto reload on alarm?
 * timer_interval_sec - the interval of alarm to set
 */
void sensor_timer_init(int timer_idx,
                       bool auto_reload, double timer_freq)
{
    /* Select and initialize basic parameters of the timer */
    timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_EN;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = auto_reload;
    timer_init(TIMER_GROUP_0, timer_idx, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP_0, timer_idx, 0x00000000ULL);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(TIMER_GROUP_0, timer_idx, TIMER_SCALE / timer_freq);
    timer_enable_intr(TIMER_GROUP_0, timer_idx);
    timer_isr_register(TIMER_GROUP_0, timer_idx, timer_group0_isr,
                       (void *)timer_idx, ESP_INTR_FLAG_IRAM, NULL);

    timer_start(TIMER_GROUP_0, timer_idx);
}

void app_main()
{
    
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    //Initialize sensors
    initSensors();

//#ifdef CONFIG_ETHERNET_ENABLE
    //ret = initEthernet();
//#endif
//#ifdef CONFIG_WIFI_ENABLE
    //ret = initWiFi();
//#endif
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());
/* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */



#if CONFIG_UDP_CLIENT_ENABLE
       xTaskCreate(udp_client_task, "udp_client", 4096, NULL, 5, NULL);

#elif CONFIG_TCP_CLIENT_ENABLE  
       xTaskCreate(tcp_client_task, "tcp_client", 4096, NULL, 5, NULL);   
#endif

    ws_server_start();
    xTaskCreate(&server_task,"server_task",3000,NULL,7,NULL);
    xTaskCreate(&server_handle_task,"server_handle_task",4000,NULL,6,NULL);

    //Creat a binary semaphore to read the sensor frequently
    xSemaphore = xSemaphoreCreateBinary();
    //Initialize hw timer
    sensor_timer_init(TIMER_INDEX, true, TIMER_FREQ);

    //vTaskDelay(2000/portTICK_RATE_MS);                                             
}
