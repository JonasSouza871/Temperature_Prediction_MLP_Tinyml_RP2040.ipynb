#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa TFLM e carrega modelo de temperatura, retorna 0 se OK
int tflm_init(void);

// Ponteiro pro buffer de entrada float32[10][4] = 40 floats
// (10 timesteps × 4 features: Temp_AHT20, Umid_AHT20, Temp_BMP280, Press_BMP280)
float* tflm_input_ptr(int* nfloats);

// Ponteiro pro buffer de saída float32[3]
// (3 previsões: temperatura em 5min, 10min, 15min)
float* tflm_output_ptr(int* nfloats);

// Executa inferência, retorna 0 se OK
int tflm_invoke(void);

// Retorna bytes usados da arena (debug)
int tflm_arena_used_bytes(void);

#ifdef __cplusplus
}
#endif
