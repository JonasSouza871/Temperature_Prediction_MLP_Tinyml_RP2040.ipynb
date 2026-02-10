#include "tflm_wrapper.h"
#include "temperature_model.h" //modelo CNN 1D embarcado na flash
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include <stdio.h>

static constexpr int kTensorArenaSize = 60 * 1024;
alignas(16) static uint8_t tensor_arena[kTensorArenaSize]; //alinhado em 16 bytes para performance

static const tflite::Model*    model_ptr       = nullptr;
static tflite::MicroInterpreter* interpreter_ptr = nullptr;
static TfLiteTensor* input_ptr  = nullptr; //tensor de entrada [1, 10, 4] float32
static TfLiteTensor* output_ptr = nullptr; //tensor de saída [1, 3] float32

extern "C" int tflm_init(void) {
    printf("[TFLM] Carregando modelo...\n");
    model_ptr = tflite::GetModel(temperature_model);
    if (!model_ptr) {
        printf("[TFLM] ERRO: Modelo nao encontrado!\n");
        return 1;
    }
    printf("[TFLM] Modelo carregado OK\n");

    printf("[TFLM] Verificando versao do schema...\n");
    if (model_ptr->version() != TFLITE_SCHEMA_VERSION) {
        printf("[TFLM] ERRO: Schema v%d, esperado v%d\n", model_ptr->version(), TFLITE_SCHEMA_VERSION);
        return 2;
    }
    printf("[TFLM] Schema OK (v%d)\n", TFLITE_SCHEMA_VERSION);

    static tflite::MicroMutableOpResolver<11> resolver;
    resolver.AddConv2D();        //Conv1D implementado como Conv2D com width=1
    resolver.AddMean();          //GlobalAveragePooling1D
    resolver.AddFullyConnected();//camadas densas
    resolver.AddReshape();       //reshape entre camadas
    resolver.AddQuantize();      //quantização
    resolver.AddDequantize();    //dequantização
    resolver.AddRelu();          //ativação ReLU
    resolver.AddPad();           //padding
    resolver.AddMaxPool2D();     //max pooling
    resolver.AddSoftmax();       //softmax
    resolver.AddExpandDims();    //ExpandDims

    printf("[TFLM] Criando interpretador (arena=%d KB)...\n", kTensorArenaSize / 1024);
    static tflite::MicroInterpreter static_interpreter(model_ptr, resolver, tensor_arena, kTensorArenaSize);
    interpreter_ptr = &static_interpreter;
    printf("[TFLM] Interpretador criado OK\n");

    printf("[TFLM] Alocando tensores...\n");
    if (interpreter_ptr->AllocateTensors() != kTfLiteOk) {
        printf("[TFLM] ERRO: AllocateTensors falhou!\n");
        return 3;
    }
    printf("[TFLM] Tensores alocados OK\n");

    printf("[TFLM] Obtendo ponteiros dos tensores...\n");
    input_ptr  = interpreter_ptr->input(0);
    output_ptr = interpreter_ptr->output(0);
    if (!input_ptr || !output_ptr) {
        printf("[TFLM] ERRO: Tensores nulos!\n");
        return 4;
    }
    printf("[TFLM] Input: %p, Output: %p\n", input_ptr, output_ptr);

    printf("[TFLM] Validando tipos dos tensores...\n");
    printf("[TFLM] Input type: %d (esperado: %d=float32)\n", input_ptr->type, kTfLiteFloat32);
    printf("[TFLM] Output type: %d (esperado: %d=float32)\n", output_ptr->type, kTfLiteFloat32);
    if (input_ptr->type != kTfLiteFloat32) {
        printf("[TFLM] ERRO: Tipo do input incorreto!\n");
        return 5;
    }
    if (output_ptr->type != kTfLiteFloat32) {
        printf("[TFLM] ERRO: Tipo do output incorreto!\n");
        return 6;
    }

    printf("[TFLM] Inicializacao completa! Arena usado: %d bytes\n", (int)interpreter_ptr->arena_used_bytes());
    return 0;
}

extern "C" float* tflm_input_ptr(int* nfloats) {
    if (!input_ptr) return nullptr;
    if (nfloats) *nfloats = input_ptr->bytes / sizeof(float); //40 floats: 10 timesteps * 4 features
    return input_ptr->data.f;
}

extern "C" float* tflm_output_ptr(int* nfloats) {
    if (!output_ptr) return nullptr;
    if (nfloats) *nfloats = output_ptr->bytes / sizeof(float); //3 floats: previsões 5, 10, 15 min
    return output_ptr->data.f;
}

extern "C" int tflm_invoke(void) {
    if (!interpreter_ptr) return 1;
    return (interpreter_ptr->Invoke() == kTfLiteOk) ? 0 : 2;
}

extern "C" int tflm_arena_used_bytes(void) {
    if (!interpreter_ptr) return -1;
    return (int)interpreter_ptr->arena_used_bytes();
}
