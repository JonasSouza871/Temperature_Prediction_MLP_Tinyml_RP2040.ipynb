#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "tflm_wrapper.h"
#include "scaler_params.h" //parâmetros de normalização (mean e scale) gerados no treino
#include "ssd1306.h"
#include "font.h"
#include "aht20.h"
#include "bmp280.h"

#define WINDOW_SIZE        10    //tamanho da janela temporal usada pelo modelo
#define NUM_FEATURES       4     //Temp_AHT20, Umid_AHT20, Temp_BMP280, Press_BMP280
#define NUM_HORIZONS       3     //número de previsões: 5, 10 e 15 minutos
#define SAMPLE_INTERVAL_MS 31000 //intervalo entre coletas em ms

ssd1306_t display;

static float sensor_window[WINDOW_SIZE][NUM_FEATURES]; //buffer circular: 10 amostras × 4 features
static int window_pos = 0;
static bool window_full = false;
static struct bmp280_calib_param bmp_params; //calibração lida uma vez na inicialização

//lê temperatura e umidade do AHT20, retorna 0 se OK
int read_aht20(float* temp_c, float* humidity_pct) {
    AHT20_Data dados_aht;
    if (!aht20_read(i2c0, &dados_aht)) return -1;
    *temp_c = dados_aht.temperature;
    *humidity_pct = dados_aht.humidity;
    return 0;
}

//lê temperatura e pressão do BMP280, aplica calibração, retorna 0 se OK
int read_bmp280(float* temp_c, float* pressure_hpa) {
    int32_t temp_raw, press_raw;
    bmp280_read_raw(i2c0, &temp_raw, &press_raw);
    int32_t temp_conv  = bmp280_convert_temp(temp_raw, &bmp_params);
    int32_t press_conv = bmp280_convert_pressure(press_raw, temp_raw, &bmp_params);
    *temp_c       = temp_conv / 100.0f;  //°C
    *pressure_hpa = press_conv / 100.0f; //Pa -> hPa
    return 0;
}

//normalização z-score com parâmetros do scaler treinado: (x - mean) / scale
void normalize_feature(float* feature, int feature_idx) {
    *feature = (*feature - scaler_mean[feature_idx]) / scaler_scale[feature_idx];
}

//monta o tensor de entrada, executa inferência e exibe previsões no serial e display
void run_temperature_prediction(void) {
    int in_size, out_size;
    float* input  = tflm_input_ptr(&in_size);
    float* output = tflm_output_ptr(&out_size);
    if (!input || !output) {
        printf("ERRO: Tensores inválidos\n");
        return;
    }
    printf("\n=== Nova Predição ===\n");
    for (int t = 0; t < WINDOW_SIZE; t++) { //preenche tensor [1, 10, 4]
        for (int f = 0; f < NUM_FEATURES; f++)
            input[t * NUM_FEATURES + f] = sensor_window[t][f];
    }
    int rc = tflm_invoke(); //executa CNN 1D
    if (rc != 0) {
        printf("ERRO tflm_invoke: %d\n", rc);
        return;
    }
    printf("Previsões de Temperatura (AHT20):\n");
    printf("  +5 min:  %.2f °C\n", output[0]);
    printf("  +10 min: %.2f °C\n", output[1]);
    printf("  +15 min: %.2f °C\n", output[2]);
    ssd1306_fill(&display, false);
    char line[24];
    snprintf(line, sizeof(line), "Temp Prediction");
    ssd1306_draw_string(&display, line, 0, 0, false);
    snprintf(line, sizeof(line), "+5m:  %.2fC", output[0]);
    ssd1306_draw_string(&display, line, 0, 16, false);
    snprintf(line, sizeof(line), "+10m: %.2fC", output[1]);
    ssd1306_draw_string(&display, line, 0, 28, false);
    snprintf(line, sizeof(line), "+15m: %.2fC", output[2]);
    ssd1306_draw_string(&display, line, 0, 40, false);
    ssd1306_send_data(&display);
}

//coleta uma amostra dos sensores, normaliza e insere no buffer circular
int collect_sensor_sample(void) {
    float temp_aht20, humidity_aht20, temp_bmp280, pressure_bmp280;
    if (read_aht20(&temp_aht20, &humidity_aht20) != 0) {
        printf("ERRO: Falha ao ler AHT20\n");
        return -1;
    }
    if (read_bmp280(&temp_bmp280, &pressure_bmp280) != 0) {
        printf("ERRO: Falha ao ler BMP280\n");
        return -1;
    }
    printf("Sensores: AHT20=%.2f°C %.2f%% | BMP280=%.2f°C %.2fhPa\n",
           temp_aht20, humidity_aht20, temp_bmp280, pressure_bmp280);
    sensor_window[window_pos][0] = temp_aht20;
    sensor_window[window_pos][1] = humidity_aht20;
    sensor_window[window_pos][2] = temp_bmp280;
    sensor_window[window_pos][3] = pressure_bmp280;
    for (int f = 0; f < NUM_FEATURES; f++)
        normalize_feature(&sensor_window[window_pos][f], f);
    window_pos = (window_pos + 1) % WINDOW_SIZE; //avança posição no buffer circular
    if (window_pos == 0 && !window_full) {
        window_full = true; //janela cheia: predição liberada
        printf("Janela temporal completa! Iniciando predições...\n");
    }
    return 0;
}

int main() {
    stdio_init_all();
    sleep_ms(2000); //aguarda USB/serial estabilizar
    printf("Temperature Prediction - CNN 1D\n\n");

    //I2C0: sensores (GP0=SDA, GP1=SCL) a 100kHz
    i2c_init(i2c0, 100 * 1000);
    gpio_set_function(0, GPIO_FUNC_I2C);
    gpio_set_function(1, GPIO_FUNC_I2C);
    gpio_pull_up(0);
    gpio_pull_up(1);

    //I2C1: display OLED (GP14=SDA, GP15=SCL) a 400kHz
    i2c_init(i2c1, 400 * 1000);
    gpio_set_function(14, GPIO_FUNC_I2C);
    gpio_set_function(15, GPIO_FUNC_I2C);
    gpio_pull_up(14);
    gpio_pull_up(15);

    printf("Inicializando sensores...\n");
    if (!aht20_init(i2c0))
        printf("ERRO: Falha ao inicializar AHT20!\n");
    else
        printf("AHT20 OK\n");
    bmp280_init(i2c0);
    bmp280_get_calib_params(i2c0, &bmp_params); //lê calibração uma única vez
    printf("BMP280 OK\n");

    printf("Inicializando display...\n");
    ssd1306_init(&display, 128, 64, false, 0x3C, i2c1);
    ssd1306_config(&display);
    ssd1306_fill(&display, false);
    ssd1306_draw_string(&display, "TinyML Temp", 0, 0, false);
    ssd1306_draw_string(&display, "CNN 1D Model", 0, 16, false);
    ssd1306_draw_string(&display, "Inicializando...", 0, 32, false);
    ssd1306_send_data(&display);

    printf("Inicializando TFLM...\n");
    int rc = tflm_init();
    if (rc != 0) {
        printf("ERRO tflm_init: %d\n", rc);
        switch(rc) {
            case 1: printf("Modelo nao encontrado!\n"); break;
            case 2: printf("Schema incompativel!\n"); break;
            case 3: printf("AllocateTensors falhou!\n"); break;
            case 4: printf("Tensores nulos!\n"); break;
            case 5: printf("Tipo do input incorreto!\n"); break;
            case 6: printf("Tipo do output incorreto!\n"); break;
            default: printf("Erro desconhecido!\n"); break;
        }
        ssd1306_fill(&display, false);
        ssd1306_draw_string(&display, "ERROR!", 0, 0, false);
        char msg[32];
        snprintf(msg, sizeof(msg), "TFLM Init: %d", rc);
        ssd1306_draw_string(&display, msg, 0, 20, false);
        ssd1306_send_data(&display);
        while (1) tight_loop_contents();
    }
    printf("TFLM OK - Arena: %d bytes\n\n", tflm_arena_used_bytes());

    ssd1306_fill(&display, false);
    ssd1306_draw_string(&display, "PRONTO!", 0, 0, false);
    ssd1306_draw_string(&display, "Coletando dados", 0, 16, false);
    ssd1306_draw_string(&display, "janela temporal", 0, 28, false);
    ssd1306_draw_string(&display, "(10 amostras)", 0, 40, false);
    ssd1306_send_data(&display);

    printf("Coletando a cada %d s, aguardando 10 amostras...\n\n", SAMPLE_INTERVAL_MS/1000);

    absolute_time_t last_sample_time = get_absolute_time();
    while (1) {
        int64_t elapsed_ms = absolute_time_diff_us(last_sample_time, get_absolute_time()) / 1000;
        if (elapsed_ms >= SAMPLE_INTERVAL_MS) {
            if (collect_sensor_sample() == 0) {
                if (window_full)
                    run_temperature_prediction();
                else
                    printf("Amostras coletadas: %d/10\n", window_pos);
            }
            last_sample_time = get_absolute_time();
        }
        sleep_ms(100); //evita busy-wait
    }
    return 0;
}
