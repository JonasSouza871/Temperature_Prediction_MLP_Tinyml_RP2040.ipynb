# Firmware - Temperature Prediction

Este diretório contém o código firmware para execução no microcontrolador RP2040 (Raspberry Pi Pico), incluindo a implementação da CNN 1D para predição de temperatura usando TensorFlow Lite Micro.

## Arquivos principais

- `main.c`: Código principal do firmware (template para adaptação)
- `tflm_wrapper.cpp`: Wrapper para integração com TensorFlow Lite Micro
- `tflm_wrapper.h`: Cabeçalho do wrapper TFLM
- `temperature_model.h`: Modelo CNN 1D convertido para array C
- `scaler_params.h`: Parâmetros de normalização (média e escala)
- `lib/`: Bibliotecas auxiliares (display OLED, fontes)

## Modelo

- **Arquitetura**: CNN 1D (Conv1D + Dense)
- **Input**: [1, 10, 4] - 10 timesteps × 4 features (Temp_AHT20, Umid_AHT20, Temp_BMP280, Press_BMP280)
- **Output**: [1, 3] - 3 previsões de temperatura (5min, 10min, 15min)
- **Tamanho**: ~8.6 KB (compatível com RP2040)
- **Tipo de dados**: float32

## Próximos passos (TODO)

1. Implementar funções `read_aht20()` e `read_bmp280()` no [main.c](main.c)
2. Adicionar bibliotecas dos sensores AHT20 e BMP280
3. Configurar CMakeLists.txt para compilação
4. Testar no hardware