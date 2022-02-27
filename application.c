#include "application.h"
#include "parser.h"
#include "protocol.h"
#include "checksum.h"

#include "neuton.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))

static app_callbacks_t cb = { 0 };
static int bias_column = 1;

static void app_on_valid_packet(void* data, uint32_t size);

uint8_t app_init(app_callbacks_t* callbacks)
{
	if (!callbacks)
		return 1;

	cb = *callbacks;
	return !cb.send_data ||
			parser_init(app_on_valid_packet, bias_column + MAX(neuton_model_inputs_count(),
						neuton_model_outputs_count()));
}

void app_on_receive(void* data, size_t size)
{
	uint8_t* bytes = data;
	for (size_t i = 0; i < size; i++)
		parser_parse(bytes[i]);
}

static int app_make_packet(PacketHeader* hdr, void* payload, size_t size, PacketType type, ErrorCode err)
{
	if (!hdr || ((size + sizeof(PacketHeader) + sizeof(uint16_t)) > (parser_buffer_size())))
		return 1;

	if (size && !payload)
		return 2;

	hdr->preamble = PREAMBLE;
	hdr->type = ANS(type);
	hdr->error = err;
	hdr->size = sizeof(PacketHeader) + size + sizeof(uint16_t);

	if (size)
		memcpy(hdr + 1, payload, size);

	uint16_t crc = crc16_table((uint8_t*) hdr, hdr->size - sizeof(uint16_t), 0);
	memcpy((uint8_t*) hdr + hdr->size - sizeof(uint16_t), &crc, sizeof(uint16_t));

	return 0;
}

static void app_on_valid_packet(void* data, uint32_t size)
{
	PacketHeader* hdr = (PacketHeader*) data;

	if (IS_ANS(hdr->type))
		return;

	hdr->size -= sizeof(PacketHeader) + sizeof(uint16_t);

	switch (PACKET_TYPE(hdr->type))
	{
	case TYPE_MODEL_INFO:
	{
		ModelInfo info = { 0 };
		info.taskType = neuton_model_task_type();
		info.columnsCount = neuton_model_outputs_count();

		if (0 == app_make_packet(hdr, &info, sizeof(info), TYPE_MODEL_INFO, ERROR_SUCCESS))
			cb.send_data(data, hdr->size);

		neuton_model_reset_inputs();
		break;
	}

	case TYPE_DATASET_INFO:
	{
		if (hdr->size < sizeof(DatasetInfo))
			break;

		DatasetInfo* info = (DatasetInfo*) (hdr + 1);

		if (info->columnsCount == (bias_column + neuton_model_inputs_count()))
		{
			if (0 == app_make_packet(hdr, NULL, 0, TYPE_DATASET_INFO, ERROR_SUCCESS))
				cb.send_data(data, hdr->size);
		}
		else
		{
			if (0 == app_make_packet(hdr, NULL, 0, TYPE_ERROR, ERROR_INVALID_SIZE))
				cb.send_data(data, hdr->size);
		}

		neuton_model_reset_inputs();
		break;
	}

	case TYPE_DATASET_SAMPLE:
	{
		float* sample = (float*) (hdr + 1);
		float* result = cb.on_dataset_sample ? cb.on_dataset_sample(sample) : NULL;
		if (0 == app_make_packet(hdr, result, result ? neuton_model_outputs_count() * sizeof(float) : 0, 
					TYPE_DATASET_SAMPLE, ERROR_SUCCESS))
			cb.send_data(data, hdr->size);

		break;
	}

	case TYPE_PERF_REPORT:
	{
		PerformanceReport report = { 0 };

		report.ramUsage = neuton_model_ram_usage();
		report.ramUsageCur = neuton_model_ram_usage();
		report.bufferSize = parser_buffer_size();
		report.flashUsage = neuton_model_size_with_meta();
		report.freq = cb.get_cpu_freq? cb.get_cpu_freq() : 0;

		if (cb.get_time_report)
			cb.get_time_report(&report.usSampleMin, &report.usSampleMax, &report.usSampleAvg);

		if (0 == app_make_packet(hdr, &report, sizeof(report), TYPE_PERF_REPORT, ERROR_SUCCESS))
			cb.send_data(data, hdr->size);

		break;
	}

	case TYPE_ERROR:
	default:
		break;
	}
}
