#include "dma.h"

#include <doca_error.h>
#include <doca_log.h>

#include <stdexcept>

namespace doca {

DOCA_LOG_REGISTER(DOCA_DMA);

doca_error_t check_dev_dma_capable(struct doca_devinfo *devinfo) {
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
        if (result != DOCA_SUCCESS) DOCA_LOG_ERR("Failed to destroy work queue: %s", doca_get_error_string(result));
        workq = NULL;

        result = doca_dma_destroy(dma_ctx);
        if (result != DOCA_SUCCESS) DOCA_LOG_ERR("Failed to destroy dma: %s", doca_get_error_string(result));
        dma_ctx = NULL;
        ctx = NULL;

        result = doca_buf_inventory_destroy(buf_inv);
        if (result != DOCA_SUCCESS) DOCA_LOG_ERR("Failed to destroy buf inventory: %s", doca_get_error_string(result));
        buf_inv = NULL;
    }

    dev.reset();
}

doca_error_t DOCADma::Init(MemMap &mmap) {
    doca_error_t result;

    result = dev->AddMMap(mmap);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to add device to mmap: %s", doca_get_error_string(result));
        return result;
    }

    if (mode == DOCA_MODE_HOST) return DOCA_SUCCESS;

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

void DOCADma::Finalize() {
    doca_error_t result;

	result = doca_ctx_workq_rm(ctx, workq);
	if (result != DOCA_SUCCESS)
		DOCA_LOG_ERR("Failed to remove work queue from ctx: %s", doca_get_error_string(result));

	result = doca_ctx_stop(ctx);
	if (result != DOCA_SUCCESS)
		DOCA_LOG_ERR("Unable to stop DMA context: %s", doca_get_error_string(result));

	result = doca_ctx_dev_rm(ctx, dev->dev);
	if (result != DOCA_SUCCESS)
		DOCA_LOG_ERR("Failed to remove device from DMA ctx: %s", doca_get_error_string(result));
}

doca_error_t DOCADma::ExportDesc(MemMap &mmap, CommChannel &ch) {
    if (mode == DOCA_MODE_DPU) {
        DOCA_LOG_ERR("DPU should not export memory");
        return DOCA_SUCCESS;
    }

    doca_error_t result;

    result = mmap.ExportDPU(*dev);
    if (result != DOCA_SUCCESS) return result;
    result = mmap.SendDesc(ch);
    if (result != DOCA_SUCCESS) return result;
    result = ch.WaitForSuccessfulMsg();
    return result;
}

doca_error_t DOCADma::AddBuffer(MemMap &local_mmap, MemMap &remote_mmap) {
    doca_error_t result;
    /* Construct DOCA buffer for remote (Host) address range */
    result = doca_buf_inventory_buf_by_addr(buf_inv, remote_mmap.mmap, remote_mmap.buffer, remote_mmap.len,
                                            &remote_mmap.doca_buf);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to acquire DOCA remote buffer: %s", doca_get_error_string(result));
        return result;
    }
    /* Construct DOCA buffer for local (DPU) address range */
    result = doca_buf_inventory_buf_by_addr(buf_inv, local_mmap.mmap, local_mmap.buffer, local_mmap.len,
                                            &local_mmap.doca_buf);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to acquire DOCA local buffer: %s", doca_get_error_string(result));
        return result;
    }

    return result;
}

void DOCADma::RmBuffer(MemMap &local_mmap, MemMap &remote_mmap) {
    doca_buf_refcount_rm(local_mmap.doca_buf, NULL);
    doca_buf_refcount_rm(remote_mmap.doca_buf, NULL);
}

doca_error_t DOCADma::DmaCopy(MemMap &from, MemMap &to, size_t size) {
    doca_error_t result;

    struct doca_event event = {0};
    struct doca_dma_job_memcpy dma_job = {0};

    /* Construct DMA job */
    dma_job.base.type = DOCA_DMA_JOB_MEMCPY;
    dma_job.base.flags = DOCA_JOB_FLAGS_NONE;
    dma_job.base.ctx = ctx;

    dma_job.src_buff = from.doca_buf;
    dma_job.dst_buff = to.doca_buf;

    /* Enqueue DMA job */
	result = doca_workq_submit(workq, &dma_job.base);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to submit DMA job: %s", doca_get_error_string(result));
		return result;
	}

    /* Wait for job completion */
	while ((result = doca_workq_progress_retrieve(workq, &event, DOCA_WORKQ_RETRIEVE_FLAGS_NONE)) ==
	       DOCA_ERROR_AGAIN) {
		;
	}

    if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to retrieve DMA job: %s", doca_get_error_string(result));
		return result;
	}

	/* event result is valid */
	result = (doca_error_t)event.result.u64;
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("DMA job event returned unsuccessfully: %s", doca_get_error_string(result));
		return result;
	}

    return result;
}

}  // namespace doca
