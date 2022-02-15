/*
 *   ESP_SENSOR config
 */
#include "stdio.h"
#include "esp_sensor.h"
#include "driver/i2c.h"
#include "esp_i2c.h"
#include "esp_log.h"
#include "driver/timer.h"


#define SENSOR_TAG "ESP_SENSOR"

#define I2C_NUM I2C_NUM_0 /*!< I2C port number for master dev */

#define READ_SENSOR_DATA_LENGTH CONFIG_READ_SENSOR_DATA_LENGTH /*!< Each time read n bytes from sensor*/
#define MAX_SENSOR_ADDR CONFIG_MAX_SENSOR_ADDR                 /*!< Maximum address of sensors*/
#define MAX_SENSOR_NUM CONFIG_MAX_SENSOR_NUM                   /*!< Maximum number of sensors*/
#define MOVING_AVG_LENGTH 5

#define SENSOR_RESET_IO CONFIG_SENSOR_RESET_IO
#define SENSOR_IRQ_IO CONFIG_SENSOR_IRQ_IO

uint8_t sensor_table[MAX_SENSOR_NUM];
uint8_t sensor_table_length;

const uint8_t REG_OFFSET = 0x30;          /*!< offset of config registers ?????*/
const uint8_t SENSOR_CONFIGS_LENGTH = 15; /*!< bytes of config registers */


/*  const unsigned char init_sensor_reg_tb[][2] =
    {
        {0x42, 0x00}, //reg[66] Electrode Configuration EG00
        {0x43, 0x00}, //reg[67] Electrode Configuration EG01
        {0x44, 0x00}, //reg[68] Electrode Configuration EG02
        {0x45, 0x00}, //reg[69] Electrode Configuration EG03
        {0x46, 0x00}, //reg[70] Electrode Configuration EG04
        {0x47, 0x00}, //reg[71] Electrode Configuration EG05
        {0x48, 0x00}, //reg[72] Electrode Configuration EG06
        {0x49, 0x00}, //reg[73] Electrode Configuration EG07
        {0x4A, 0x00}, //reg[74] Electrode Configuration EG08
        {0x4B, 0x00}, //reg[75] Electrode Configuration EG09
        {0x4C, 0x00}, //reg[76] Not used
        {0x4D, 0x00}, //reg[77] signal processing
        {0x4E, 0x00}, //reg[78] MeasureMENt confoguration
        {0X4F, 0x00}, //reg[79] Frequencies
        {0X50, 0x00}, //reg[80] Flags
};  */
 const unsigned char init_sensor_reg_tb[][2] =
    {
        {0x42, 0x01},//reg[66] Electrode Configuration EG00
        {0x43, 0x03}, //reg[67] Electrode Configuration EG01
        {0x44, 0x00}, //reg[68] Electrode Configuration EG02
        {0x45, 0x00}, //reg[69] Electrode Configuration EG03
        {0x46, 0x00}, //reg[70] Electrode Configuration EG04
        {0x47, 0x00}, //reg[71] Electrode Configuration EG05
        {0x48, 0x00}, //reg[72] Electrode Configuration EG06
        {0x49, 0x00}, //reg[73] Electrode Configuration EG07
        {0x4A, 0x00}, //reg[74] Electrode Configuration EG08
        {0x4B, 0x00}, //reg[75] Electrode Configuration EG09
        {0x4C, 0x00}, //reg[76] Not used
        {0x4D, 0x12}, //reg[77] signal processing
        {0x4E, 0x98}, //reg[78] MeasureMENt confoguration
        {0x4F, 0x07}, //reg[79] Frequencies
        {0x50, 0xA0} //reg[80] Flags
}; 

/**
 * @brief   GPIO init function
 * 
 * GPIO status:
 * GPIO7: output - reset pin
 * GPIO9: input  - irq pin
 */
static void sensor_gpio_init(){
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
     io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set

    io_conf.pin_bit_mask = (1ULL<<SENSOR_RESET_IO);
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //enable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    gpio_set_level(SENSOR_RESET_IO, 1);

    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_INPUT;
    //bit mask of the pins that you want to set
    io_conf.pin_bit_mask = (1ULL<<SENSOR_IRQ_IO);
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    //configure GPIO with the given settings
    gpio_config(&io_conf);   
}

/**
 * @brief   reset all sensor devices
 */
void reset_sensor(){
    gpio_set_level(SENSOR_RESET_IO, 0);
    vTaskDelay(5/portTICK_RATE_MS);
    gpio_set_level(SENSOR_RESET_IO, 1);
}

/**
 * @brief   get irq state
 */
uint8_t get_sensor_irq_state(){
    return gpio_get_level(SENSOR_IRQ_IO);
}


/**
 * @brief   Find sensor devices
 * 
 * @param   sensor_table[]: used for saving the address of founded devices
 *          sensor_table_length: number of founded devices
 * 
 * @return  sensor_table_length
 */
 uint8_t find_sensor_devices()
{
    uint8_t address = 0x00;
    uint8_t sensor_num = 0;

    esp_err_t ret = ESP_OK;
    for (uint8_t i = 0; i < MAX_SENSOR_ADDR; i++)
    {
        uint8_t temp;
        ret = i2c_read_sensor(I2C_NUM, address, 0, &temp, 1);
        if (ret == ESP_OK)
        {
            ESP_LOGI(SENSOR_TAG, "Find sensor device at the address: %x", address);
            sensor_table[sensor_num] = address;
            sensor_num++;
            if (sensor_num > MAX_SENSOR_NUM)
            {
                break;
            }
        }
        address++;
    }
    return sensor_num;
}

/**
 * @brief   read sensor data
 */
esp_err_t read_sensor_data(uint8_t devs, uint8_t reg, uint8_t *out, uint8_t length)
{
    esp_err_t ret = ESP_OK;
    if (sensor_table_length != 0 && devs == 0xff)
    {
        for (int i = 0; i < sensor_table_length; i++)
            ret = i2c_read_sensor(I2C_NUM, sensor_table[i], reg, out + i * length, length);
            
    }
    else if(devs < sensor_table_length)
    {
        ESP_LOGI("read_sensor_data:", "I2C_NUM %x", I2C_NUM);
        ESP_LOGI("read_sensor_data:", "sensor_table[devs] %x", sensor_table[devs]);
        ESP_LOGI("read_sensor_data:", "reg %x", reg);
        ESP_LOGI("read_sensor_data:", "length %x", length);
        ret = i2c_read_sensor(I2C_NUM, sensor_table[devs], reg, out, length);
    }
    
    return ret;
}

/**
 * @brief   read sensor config
 */
esp_err_t read_sensor_configs(uint8_t sensor_addr, uint8_t *data)
{
    esp_err_t ret = ESP_OK;
    if (sensor_table_length != 0)
    {
        ret = i2c_read_sensor(I2C_NUM, sensor_addr, REG_OFFSET, data, SENSOR_CONFIGS_LENGTH);
    }
    return ret;
}

/**
 * @brief   set sensor config
 */
esp_err_t set_sensor_configs(uint8_t sensor_addr, uint8_t *configs, uint8_t len)
{
    esp_err_t ret = ESP_FAIL;
    if (len != SENSOR_CONFIGS_LENGTH)
        return ret;

    for (int8_t i = 0; i < SENSOR_CONFIGS_LENGTH; i++)
    {
        ret = i2c_write_sensor(I2C_NUM,
                               sensor_addr, init_sensor_reg_tb[i][0], configs[i]);
        if (ret != ESP_OK)
            ESP_LOGE(SENSOR_TAG, "Sensor at the address %x inited not correct!", sensor_addr);
        else
        {
            ESP_LOGI(SENSOR_TAG, "Initializing the sensor %x, write %x into %x...",
                     sensor_addr, configs[i], init_sensor_reg_tb[i][0]);
        }
    }
    return ret;
}

/**
 * @brief   initialize sensor devices with the default config
 */
void initSensors()
{
    sensor_gpio_init();
    esp_err_t ret = ESP_OK;

    ret = i2c_master_init(I2C_NUM);
    if (ret != ESP_OK)
    {
        ESP_LOGE(SENSOR_TAG, "I2C inited failed");
        return;
    }
    sensor_table_length = find_sensor_devices();
    if (sensor_table_length == 0)
    {
        ESP_LOGE(SENSOR_TAG, "No sensor device was found!");
        return;
    }

    for (int8_t i = 0; i < sensor_table_length; i++)
    {
        for (int8_t j = 0; j < SENSOR_CONFIGS_LENGTH; j++)
        {
            ret = i2c_write_sensor(I2C_NUM,
                                   sensor_table[i], init_sensor_reg_tb[j][0], init_sensor_reg_tb[j][1]);
            if (ret != ESP_OK)
            {
                ESP_LOGE(SENSOR_TAG, "Sensor at the address %x inited not correct!", sensor_table[i]);
            }
            else
            {
                ESP_LOGI(SENSOR_TAG, "Initializing the sensor %x, write %x into %x...",
                         sensor_table[i], init_sensor_reg_tb[j][1], init_sensor_reg_tb[j][0]);
            }
        }
    }
}

void merge_data(uint8_t *input, uint16_t *output, uint8_t data_length)
{
    uint8_t merged_length = data_length / 2;
    uint16_t merged_data[merged_length];

    uint8_t mergedIdx = 0;
    for (uint8_t dataIdx = 1; dataIdx < data_length; dataIdx = dataIdx + 2)
    {
        merged_data[mergedIdx] = ((uint16_t)input[dataIdx] << 8) | (uint16_t)input[dataIdx - 1];
        mergedIdx++;
    }
    if (data_length % 2 == 1)
    {
        merged_data[merged_length] = (uint16_t)input[data_length - 1];
        mergedIdx++;
    }
    // copy data
    for (uint16_t outputIdx = 0; outputIdx < mergedIdx; ++outputIdx)
    {
        output[outputIdx] = merged_data[outputIdx];
        //ESP_LOGI("MERGEDATA","output %x",output[outputIdx]);
    }
}

uint16_t moving_average(uint16_t *ptrBuffer, int32_t *ptrSum, uint8_t pos, uint16_t nextNum)
{
    //Subtract the oldest number from the prev sum, add the new number
    *ptrSum = *ptrSum + nextNum - ptrBuffer[pos];
    //Assign the nextNum to the position in the array
    ptrBuffer[pos] = nextNum;
    //return the average
    return *ptrSum / MOVING_AVG_LENGTH;
}
