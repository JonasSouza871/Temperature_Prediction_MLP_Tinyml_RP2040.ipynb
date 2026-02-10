#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int tflm_init(void); //inicializa TFLM e carrega modelo, retorna 0 se OK
float* tflm_input_ptr(int* nfloats); //buffer de entrada float32[10][4] = 40 floats
float* tflm_output_ptr(int* nfloats); //buffer de saída float32[3]: previsões 5, 10, 15 min
int tflm_invoke(void); //executa inferência, retorna 0 se OK
int tflm_arena_used_bytes(void); //bytes usados da arena

#ifdef __cplusplus
}
#endif
