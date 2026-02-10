#ifndef AHT20_H
#define AHT20_H

#include <stdbool.h>
#include "hardware/i2c.h"

#define AHT20_I2C_ADDR      0x38
#define AHT20_CMD_INIT          0xBE
#define AHT20_CMD_TRIGGER       0xAC
#define AHT20_CMD_RESET         0xBA
#define AHT20_STATUS_BUSY       0x80 //bit: sensor ocupado
#define AHT20_STATUS_CALIBRATED 0x08 //bit: sensor calibrado

typedef struct {
    float temperature;
    float humidity;
} AHT20_Data;

bool aht20_init(i2c_inst_t *i2c);                   //inicializa o sensor
bool aht20_read(i2c_inst_t *i2c, AHT20_Data *data); //lê temperatura e umidade
void aht20_reset(i2c_inst_t *i2c);                  //soft reset
bool aht20_check(i2c_inst_t *i2c);                  //verifica se sensor responde no I2C

#endif //AHT20_H
