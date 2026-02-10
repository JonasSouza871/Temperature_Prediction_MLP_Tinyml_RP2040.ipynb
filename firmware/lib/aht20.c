#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "aht20.h"

bool aht20_init(i2c_inst_t *i2c) {
    uint8_t init_cmd[3] = {AHT20_CMD_INIT, 0x08, 0x00};
    i2c_write_blocking(i2c, AHT20_I2C_ADDR, init_cmd, 3, false); //envia comando de inicialização
    sleep_ms(50); //aguarda sensor estabilizar

    uint8_t status;
    for (int i = 0; i < 10; i++) {
        i2c_read_blocking(i2c, AHT20_I2C_ADDR, &status, 1, false); //lê byte de status
        if ((status & AHT20_STATUS_CALIBRATED) == AHT20_STATUS_CALIBRATED)
            return true; //sensor calibrado e pronto
        sleep_ms(10);
    }
    return false; //timeout: sensor não calibrou
}

bool aht20_read(i2c_inst_t *i2c, AHT20_Data *data) {
    uint8_t trigger_cmd[3] = {AHT20_CMD_TRIGGER, 0x33, 0x00};
    uint8_t buffer[6];

    i2c_write_blocking(i2c, AHT20_I2C_ADDR, trigger_cmd, 3, false); //dispara medição

    uint8_t status;
    for (int i = 0; i < 10; i++) {
        i2c_read_blocking(i2c, AHT20_I2C_ADDR, &status, 1, false); //pooling de status
        if (!(status & AHT20_STATUS_BUSY)) break; //sensor pronto
        sleep_ms(10);
    }

    if (status & AHT20_STATUS_BUSY) return false; //timeout: sensor ainda ocupado

    if (i2c_read_blocking(i2c, AHT20_I2C_ADDR, buffer, 6, false) != 6) return false; //falha na leitura

    uint32_t raw_humidity = ((uint32_t)buffer[1] << 12) |
                            ((uint32_t)buffer[2] << 4)  |
                            (buffer[3] >> 4);                          //umidade: 20 bits [1..3]
    data->humidity = (float)raw_humidity * 100.0 / 1048576.0;         //converte para %RH

    uint32_t raw_temp = ((uint32_t)(buffer[3] & 0x0F) << 16) |
                        ((uint32_t)buffer[4] << 8) |
                        buffer[5];                                     //temperatura: 20 bits [3..5]
    data->temperature = ((float)raw_temp * 200.0 / 1048576.0) - 50.0; //converte para °C

    return true;
}

void aht20_reset(i2c_inst_t *i2c) {
    uint8_t reset_cmd = AHT20_CMD_RESET;
    i2c_write_blocking(i2c, AHT20_I2C_ADDR, &reset_cmd, 1, false); //soft reset
    sleep_ms(20);
    aht20_init(i2c); //reinicializa após reset
}

bool aht20_check(i2c_inst_t *i2c) {
    uint8_t status;
    return i2c_read_blocking(i2c, AHT20_I2C_ADDR, &status, 1, false) == 1; //verifica se sensor responde no I2C
}
