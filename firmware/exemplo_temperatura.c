#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "aht20.h"      // Driver do sensor AHT20
#include "bmp280.h"     // Driver do sensor BMP280

// Configuração do barramento I2C para os sensores
#define I2C_PORT i2c0
#define I2C_SDA_PIN 0
#define I2C_SCL_PIN 1

int main() {
    stdio_init_all(); // Inicializa comunicação serial

    // Inicializa I2C para os sensores
    i2c_init(I2C_PORT, 100 * 1000); // 100 kHz
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    // Inicializa sensores
    aht20_init(I2C_PORT);
    bmp280_init(I2C_PORT);

    // Parâmetros de calibração do BMP280
    struct bmp280_calib_param params_bmp;
    bmp280_get_calib_params(I2C_PORT, &params_bmp);

    while (true) {
        // Variáveis para armazenar os valores lidos
        float temp_aht, umidade, temp_bmp, pressao;

        // Lê dados do AHT20
        AHT20_Data dados_aht;
        aht20_read(I2C_PORT, &dados_aht);
        temp_aht = dados_aht.temperature;
        umidade = dados_aht.humidity;

        // Lê dados do BMP280
        int32_t temp_raw, press_raw;
        bmp280_read_raw(I2C_PORT, &temp_raw, &press_raw);
        int32_t temp_conv = bmp280_convert_temp(temp_raw, &params_bmp);
        int32_t press_conv = bmp280_convert_pressure(press_raw, temp_raw, &params_bmp);
        temp_bmp = temp_conv / 100.0f;
        pressao = press_conv / 100.0f; // Converte Pa para hPa

        // Exibe os valores no monitor serial
        printf("Temperatura AHT20: %.2f °C\n", temp_aht);
        printf("Umidade AHT20: %.2f %%\n", umidade);
        printf("Temperatura BMP280: %.2f °C\n", temp_bmp);
        printf("Pressão BMP280: %.2f hPa\n", pressao);
        printf("------------------------------\n");

        sleep_ms(2000); // Aguarda 2 segundos antes da próxima leitura
    }

    return 0;
}
