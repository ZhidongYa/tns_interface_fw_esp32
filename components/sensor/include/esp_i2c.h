// Copyright (c) 2017 Olimex Ltd.
//
// GNU GENERAL PUBLIC LICENSE
//    Version 3, 29 June 2007

#ifndef __ESP_I2C_H__
#define __ESP_I2C_H__

#include <stdio.h>
#include "driver/i2c.h"

/**
 * @brief i2c master initialization
 */
esp_err_t i2c_master_init(uint8_t i2c_num);

/**
 * @brief test code to read esp-i2c-slave
 * ____________________________________________________________________________________________________________
 * | start | slave_addr + rd_bit +ack | write 1 byte + ack | read n-1 bytes + ack | read 1 byte + nack | stop |
 * --------|--------------------------|--------------------|----------------------|--------------------|------|
 */
esp_err_t i2c_read_sensor(i2c_port_t i2c_num,
                          uint8_t slave_addr, uint8_t offset, uint8_t *data_rd, size_t size);

/**
 * @brief Test codETHERNETe to write esp-i2c-slave
 *        Master device write data to slave(both esp32),
 *        the data will be stored in slave buffer.
 *        We can read them out from slave buffer.
 *______________________________________________________________________________________________
 * | start | slave_addr + wr_bit + ack | write 1 byte offset + ack | write 1 byte + ack | stop |
 * --------|---------------------------|---------------------------|--------------------|------|
 */
esp_err_t i2c_write_sensor(i2c_port_t i2c_num,
                          uint8_t Slave_addr, uint8_t Register, size_t Data);

#endif