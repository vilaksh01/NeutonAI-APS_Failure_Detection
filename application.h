#ifndef APPLICATION_H
#define APPLICATION_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
	void (*send_data)(void* data, size_t size);
	float* (*on_dataset_sample)(float* inputs);
	uint32_t (*get_cpu_freq)();
	void (*get_time_report)(float* min, float* max, float* avg);
}
app_callbacks_t;

uint8_t app_init(app_callbacks_t* callbacks);
void app_on_receive(void* data, size_t size);

#ifdef __cplusplus
}
#endif

#endif // APPLICATION_H
