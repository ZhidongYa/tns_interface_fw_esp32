/*
 *   ESP_SENSOR header file
 */

#ifndef __ESP_SENSOR_H__
#define __ESP_SENSOR_H__

#include "stdint.h"
#include "stdbool.h"
#include "esp_err.h"

/**
 * @brief   reset all sensor devices
 */
void reset_sensor();
uint8_t find_sensor_devices();
//void tcp_client_task(void *pvParameters)
/**
 * @brief   get irq state
 */
uint8_t get_sensor_irq_state();


/**
 * @brief   initialize sensor devices with the default Configs
 */
void initSensors();

/**
 * @brief   return the number of devices
 */
uint8_t get_sensor_num();

/**
 * @brief   return the address table of devices
 */
void get_sensor_devtable(uint8_t *dev_table);

/**
 * @brief   read sensor config
 */
esp_err_t read_sensor_configs(uint8_t sensor_addr, uint8_t *data);

/**
 * @brief   set sensor config
 */
esp_err_t set_sensor_configs(uint8_t sensor_addr, uint8_t *configs, uint8_t len);

/**
 * @brief   read sensor data
 */
esp_err_t read_sensor_data(uint8_t devs, uint8_t reg, uint8_t *data, uint8_t length);

void merge_data(uint8_t *input, uint16_t *output, uint8_t data_length);
uint16_t moving_average(uint16_t *ptrBuffer, int32_t *ptrSum, uint8_t pos, uint16_t nextNum);

#endif