#include "dma.h"

#include <doca_error.h>
#include <doca_log.h>

#include <stdexcept>

namespace doca {

DOCA_LOG_REGISTER(DOCA_DMA);

doca_error_t check_dev_dma_capable(struct doca_devinfo *devinfo)
{
	return doca_dma_job_get_supported(devinfo, DOCA_DMA_JOB_MEMCPY);
}

DOCADma::DOCADma(doca_app_mode mode) : mode(mode) {
    doca_error_t result;
    size_t num_elements = 2;

    dev = std::make_shared<DOCADevice>();
    result = dev->OpenWithCap(check_dev_dma_capable);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to open DOCA DMA capable device");
        throw std::runtime_error("Failed to open DOCA DMA capable device");
    }

    if (mode == DOCA_MODE_HOST) return;

    result = doca_buf_inventory_create(NULL, num_elements, DOCA_BUF_EXTENSION_NONE, &buf_inv);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to create buffer inventory: %s", doca_get_error_string(result));
        throw std::runtime_error("Unable to create buffer inventory");
    }

    result = doca_dma_create(&dma_ctx);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to create DMA engine: %s", doca_get_error_string(result));
        throw std::runtime_error("Unable to create DMA engine");
    }

    ctx = doca_dma_as_ctx(dma_ctx);

    result = doca_workq_create(WORKQ_DEPTH, &workq);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to create work queue: %s", doca_get_error_string(result));
        throw std::runtime_error("Unable to create work queue");
    }
}

DOCADma::~DOCADma() {
    doca_error_t result;

	if (mode == DOCA_MODE_DPU) {
		result = doca_workq_destroy(workq);
		if (result != DOCA_SUCCESS)
			DOCA_LOG_ERR("Failed to destroy work queue: %s", doca_get_error_string(result));
		workq = NULL;

		result = doca_dma_destroy(dma_ctx);
		if (result != DOCA_SUCCESS)
			DOCA_LOG_ERR("Failed to destroy dma: %s", doca_get_error_string(result));
		dma_ctx = NULL;
		ctx = NULL;

		result = doca_buf_inventory_destroy(buf_inv);
		if (result != DOCA_SUCCESS)
			DOCA_LOG_ERR("Failed to destroy buf inventory: %s", doca_get_error_string(result));
		buf_inv = NULL;
	}

    dev.reset();
}

doca_error_t DOCADma::Init(const MemMap &mmap) {
    doca_error_t result;

	result = dev->AddMMap(mmap);
    if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to add device to mmap: %s", doca_get_error_string(result));
		return result;
	}

	if (mode == DOCA_MODE_HOST)
		return DOCA_SUCCESS;

    result = doca_buf_inventory_start(buf_inv);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to start buffer inventory: %s", doca_get_error_string(result));
		return result;
	}

    result = doca_ctx_dev_add(ctx, dev->dev);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to register device with DMA context: %s", doca_get_error_string(result));
		return result;
	}

	result = doca_ctx_start(ctx);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to start DMA context: %s", doca_get_error_string(result));
		return result;
	}

	result = doca_ctx_workq_add(ctx, workq);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to register work queue with context: %s", doca_get_error_string(result));
		return result;
	}

	return result;
}

}  // namespace doca
