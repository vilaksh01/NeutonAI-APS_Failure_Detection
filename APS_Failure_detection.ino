#include "application.h"
#include "neuton.h"

static uint8_t init_failed;
static app_callbacks_t callbacks = { 0 };
uint64_t min_time = UINT64_MAX, max_time = 0, avg_time = 0;

static void send_data(void* data, size_t size)
{
  Serial.write((const char*) data, size);
}

static float* on_dataset_sample(float* inputs)
{
  if (neuton_model_set_inputs(inputs) == 0)
  {
    uint16_t index;
    float* outputs;

    uint64_t start = micros();
    if (neuton_model_run_inference(&index, &outputs) == 0)
    {
      uint64_t stop = micros();
      
      uint64_t inference_time = stop - start;
      if (inference_time > max_time)
        max_time = inference_time;
      if (inference_time < min_time)
        min_time = inference_time;

      static uint64_t nInferences = 0;
      if (nInferences++ == 0)
      {
        avg_time = inference_time;
      }
      else
      {
        avg_time = (avg_time * nInferences + inference_time) / (nInferences + 1);
      }
  
      switch (index)
      {
      case 0:
        Serial.println("0: No Failure");
        break;
      
      case 1:
        Serial.println("1: APS Failure Detected");
        break;
      
      case 2:
        Serial.println("2: Unknown");
        break;
      
      default:
        break;
      }

      return outputs;
    }
  }

  return NULL;
}

static uint32_t get_cpu_freq()
{
  return F_CPU;
}

static void get_time_report(float* min, float* max, float* avg)
{
  *min = min_time;
  *max = max_time;
  *avg = avg_time;
}

void setup() {
  Serial.begin(230400);
  while (!Serial);
  
  callbacks.send_data = send_data;
  callbacks.on_dataset_sample = on_dataset_sample;
  callbacks.get_cpu_freq = get_cpu_freq;
  callbacks.get_time_report = get_time_report;

  init_failed = app_init(&callbacks);
}

void loop() {
  if (init_failed)
    return;

  while (Serial.available() > 0)
  {
    uint8_t byte = Serial.read();
    app_on_receive(&byte, sizeof(byte));
  } 
}
