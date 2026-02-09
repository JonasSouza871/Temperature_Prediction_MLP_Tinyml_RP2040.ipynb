#include "tflm_wrapper.h"
#include "temperature_model.h"  // Modelo CNN 1D para predição de temperatura
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

// Arena de 20KB pra tensores intermediários da CNN 1D (modelo bem menor que MNIST)
static constexpr int kTensorArenaSize = 20 * 1024;
alignas(16) static uint8_t tensor_arena[kTensorArenaSize];  // alinhado em 16 bytes pra performance

static const tflite::Model* model_ptr = nullptr;
static tflite::MicroInterpreter* interpreter_ptr = nullptr;
static TfLiteTensor* input_ptr = nullptr;   // tensor de entrada [1, 10, 4] float32
static TfLiteTensor* output_ptr = nullptr;  // tensor de saída [1, 3] float32

// Inicializa TFLM e carrega modelo da flash
extern "C" int tflm_init(void) {
    model_ptr = tflite::GetModel(temperature_model);  // carrega modelo de temperatura embarcado
    if (!model_ptr) return 1;
    if (model_ptr->version() != TFLITE_SCHEMA_VERSION) return 2;  // verifica compatibilidade

    // Registra apenas as operações usadas pelo modelo CNN 1D (economiza memória)
    static tflite::MicroMutableOpResolver<6> resolver;
    resolver.AddConv2D();           // Conv1D é implementado como Conv2D com width=1
    resolver.AddMean();             // GlobalAveragePooling1D é implementado como MEAN
    resolver.AddFullyConnected();   // camadas densas
    resolver.AddReshape();          // reshape entre camadas
    resolver.AddQuantize();         // operações de quantização
    resolver.AddDequantize();       // dequantização (modelo usa float32)

    // Cria interpretador estático (evita alocação dinâmica)
    static tflite::MicroInterpreter static_interpreter(
        model_ptr, resolver, tensor_arena, kTensorArenaSize
    );
    interpreter_ptr = &static_interpreter;

    if (interpreter_ptr->AllocateTensors() != kTfLiteOk) return 3;  // aloca memória pros tensores

    input_ptr  = interpreter_ptr->input(0);   // pega referência do tensor de entrada
    output_ptr = interpreter_ptr->output(0);  // pega referência do tensor de saída
    if (!input_ptr || !output_ptr) return 4;

    // Valida que o modelo usa float32
    if (input_ptr->type != kTfLiteFloat32)  return 5;
    if (output_ptr->type != kTfLiteFloat32) return 6;

    return 0;  // sucesso
}

// Retorna ponteiro pro buffer de entrada float32[10][4] = 40 floats
extern "C" float* tflm_input_ptr(int* nfloats) {
    if (!input_ptr) return nullptr;
    if (nfloats) *nfloats = input_ptr->bytes / sizeof(float);  // 40 floats (10 timesteps * 4 features)
    return input_ptr->data.f;
}

// Retorna ponteiro pro buffer de saída float32[3]
extern "C" float* tflm_output_ptr(int* nfloats) {
    if (!output_ptr) return nullptr;
    if (nfloats) *nfloats = output_ptr->bytes / sizeof(float);  // 3 floats (previsões para 5, 10, 15 min)
    return output_ptr->data.f;
}

// Executa inferência: processa input_ptr e gera resultado em output_ptr
extern "C" int tflm_invoke(void) {
    if (!interpreter_ptr) return 1;
    return (interpreter_ptr->Invoke() == kTfLiteOk) ? 0 : 2;
}

// Retorna quantos bytes da arena estão sendo usados (útil pra debug)
extern "C" int tflm_arena_used_bytes(void) {
    if (!interpreter_ptr) return -1;
    return (int)interpreter_ptr->arena_used_bytes();
}
