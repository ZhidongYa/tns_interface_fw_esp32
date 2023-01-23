#ifdef __cplusplus
extern "C"
{
#endif

#ifndef __WEBSERVER_H__
#define __WEBSERVER_H__
 /* typedef struct {
    char **str;     //the PChar of string array
    size_t num;     //the number of string
}IString; */
    // websocket operation codes
    typedef enum
    {
        WEBSOCKET_CMD_READCONFIG = 82,
        WEBSOCKET_CMD_WRITECONFIG = 87,
        WEBSOCKET_CMD_START = 84,
        WEBSOCKET_CMD_STOP = 83,
        WEBSOCKET_CMD_DATA = 68,
        WEBSOCKET_CMD_ERROR = 69,
        WENSOCKET_CMD_RESET = 88,
    } WEBSOCKET_CMD_t;

    typedef struct
    {
        union{
            struct {
                uint8_t CMD:7;     // bits 0..  6
                uint8_t ERROR:1;   // bit  7
                uint8_t FREQ:7;    // bits 8.. 15
                uint8_t IRQ:1;     // bit 16
            } bit;
            struct {
                uint8_t ONE:8;     // bits 0..  7
                uint8_t TWO:8;     // bits 8.. 16
            } pos;
        }param;

        union{
            struct{
                uint8_t DEVS;
                uint8_t LEN;
            }sensor;
            struct{
                uint8_t ERROR_ADDR;
                uint8_t ERROR_MSG;
            }err;
        }state;
    } user_header_t;

    // handles clients when they first connect. passes to a queue
    void server_task(void *pvParameters);

    // receives clients from queue, handles them
    void server_handle_task(void *pvParameters);

#endif //ifndef WEBSERVER_H

#ifdef __cplusplus
}
#endif
