#include <doca_argp.h>
#include <doca_error.h>
#include <doca_log.h>

#include <chrono>

#include "chan/comm_channel.h"
#include "dma/dma.h"
#include "dma_common.h"

const char *server_name = "doca_dma_server";
const int iteration = 10;

DOCA_LOG_REGISTER(DMA_SERVER::MAIN);

doca_error_t check_dev_dma_capable_1(struct doca_devinfo *devinfo) {
    return doca_dma_job_get_supported(devinfo, DOCA_DMA_JOB_MEMCPY);
}

int main(int argc, char *argv[]) {
    using namespace doca;
    using namespace std::chrono;

    doca_error_t result;
    struct dma_copy_cfg dma_cfg;
    doca_app_mode mode = DOCA_MODE_DPU;
    int64_t duration;

    struct doca_dma *dma_ctx;
    struct doca_ctx *ctx;
    struct doca_workq *workq;
    struct doca_buf_inventory *buf_inv;
    struct doca_buf *local_doca_buf, *remote_doca_buf;
    doca_mmap *local_mmap, *remote_mmap;

    /* Register a logger backend */
    result = doca_log_create_standard_backend();
    if (result != DOCA_SUCCESS) return result;

    result = doca_argp_init("doca_dma_copy", &dma_cfg);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to init ARGP resources: %s", doca_get_error_string(result));
        return result;
    }
    result = register_dma_copy_params();
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to register DMA copy server parameters: %s", doca_get_error_string(result));
        doca_argp_destroy();
        return result;
    }
    result = doca_argp_start(argc, argv);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to parse application input: %s", doca_get_error_string(result));
        return result;
    }

    CommChannel ch(mode, dma_cfg.cc_dev_pci_addr, dma_cfg.cc_dev_rep_pci_addr);

    result = ch.Listen(server_name);
    if (result != DOCA_SUCCESS) {
        doca_argp_destroy();
        return result;
    }
    result = ch.WaitForSuccessfulMsg();
    if (result != DOCA_SUCCESS) {
        doca_argp_destroy();
        return result;
    }

    size_t num_elements = 2;
    auto dev = std::make_shared<DOCADevice>();

    dev = std::make_shared<DOCADevice>();
    result = dev->OpenWithCap(check_dev_dma_capable_1);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to open DOCA DMA capable device");
        return result;
    }

    result = doca_mmap_create(nullptr, &local_mmap);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to create mmap: %s", doca_get_error_string(result));
        return result;
    }

    result = doca_buf_inventory_create(NULL, num_elements, DOCA_BUF_EXTENSION_NONE, &buf_inv);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to create buffer inventory: %s", doca_get_error_string(result));
        return result;
    }

    result = doca_dma_create(&dma_ctx);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to create DMA engine: %s", doca_get_error_string(result));
        return result;
    }

    ctx = doca_dma_as_ctx(dma_ctx);

    result = doca_workq_create(WORKQ_DEPTH, &workq);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to create work queue: %s", doca_get_error_string(result));
        return result;
    }

    result = doca_mmap_dev_add(local_mmap, dev->dev);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to add device to mmap: %s", doca_get_error_string(result));
		return result;
	}

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

    result = doca_mmap_set_permissions(local_mmap, DOCA_ACCESS_LOCAL_READ_WRITE);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to set access permissions of memory map: %s", doca_get_error_string(result));
        return result;
    }

    size_t buffer_len = dma_cfg.chunk_size;
    char *buffer = new char[buffer_len];
    if (!buffer) {
        DOCA_LOG_ERR("Failed to allocate memory for source buffer");
        return DOCA_ERROR_NO_MEMORY;
    }

    result = doca_mmap_set_memrange(local_mmap, buffer, buffer_len);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to set memrange of memory map: %s", doca_get_error_string(result));
        delete buffer;
        return result;
    }

    /* Populate local buffer into memory map to allow access from DPU side after exporting */
    result = doca_mmap_start(local_mmap);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to populate memory map: %s", doca_get_error_string(result));
        delete buffer;
    }

    ExportDesc desc;
    size_t msg_len;

    msg_len = CC_MAX_MSG_SIZE;
    result = ch.RecvFrom(desc.remote_desc, &msg_len);

    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to receive export descriptor from Host: %s", doca_get_error_string(result));
        ch.SendFailMsg();
        return result;
    }

    desc.len = msg_len;
    ch.SendSuccessfulMsg();

    result = doca_mmap_create_from_export(NULL, (const void *)desc.remote_desc, desc.len, dev->dev, &remote_mmap);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to create memory map from export descriptor");
        return result;
    }

    char *host_addr;
    uint64_t received_addr;
    size_t host_offset;

    DOCA_LOG_INFO("Waiting for Host to send address and offset");
    /* Receive remote source buffer address */
    msg_len = sizeof(received_addr);
    result = ch.RecvFrom(&received_addr, &msg_len);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to receive remote address from Host: %s", doca_get_error_string(result));
        ch.SendFailMsg();
        return result;
    }

    if (received_addr > SIZE_MAX) {
        DOCA_LOG_ERR("Address size exceeds pointer size in this device");
        ch.SendFailMsg();
        return DOCA_ERROR_INVALID_VALUE;
    }
    host_addr = (char*)received_addr;

    DOCA_LOG_INFO("Remote address received successfully from Host: %" PRIu64 "", received_addr);
    result = ch.SendSuccessfulMsg();
    if (result != DOCA_SUCCESS) return result;

    msg_len = sizeof(host_offset);
    result = ch.RecvFrom(&host_offset, &msg_len);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to receive remote address offset from Host: %s", doca_get_error_string(result));
        ch.SendFailMsg();
        return result;
    }

    if (host_offset > SIZE_MAX) {
        DOCA_LOG_ERR("Offset exceeds SIZE_MAX in this device");
        ch.SendFailMsg();
        return DOCA_ERROR_INVALID_VALUE;
    }

    DOCA_LOG_INFO("Address offset received successfully from Host: %" PRIu64 "", host_offset);

    result = ch.SendSuccessfulMsg();
    if (result != DOCA_SUCCESS) return result;

    result = doca_buf_inventory_buf_by_addr(buf_inv, remote_mmap, host_addr, host_offset,
						&remote_doca_buf);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to acquire DOCA remote buffer: %s", doca_get_error_string(result));
		ch.SendFailMsg();
		doca_mmap_destroy(remote_mmap);
		delete buffer;
		return result;
    }

    result = doca_buf_inventory_buf_by_addr(buf_inv, local_mmap, buffer, host_offset,
						&local_doca_buf);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Unable to acquire DOCA local buffer: %s", doca_get_error_string(result));
		ch.SendFailMsg();
		doca_buf_refcount_rm(remote_doca_buf, NULL);
		doca_mmap_destroy(remote_mmap);
		delete (buffer);
		return result;
	}

    struct doca_event event = {0};
	struct doca_dma_job_memcpy dma_job = {0};
	void *data;
	struct doca_buf *src_buf;
	struct doca_buf *dst_buf;
	struct timespec ts = {
		.tv_sec = 0,
		.tv_nsec = 10 * 1000,
	};

	/* Construct DMA job */
	dma_job.base.type = DOCA_DMA_JOB_MEMCPY;
	dma_job.base.flags = DOCA_JOB_FLAGS_NONE;
	dma_job.base.ctx = ctx;

    src_buf = local_doca_buf;
    dst_buf = remote_doca_buf;

	/* Set data position in src_buf */
	result = doca_buf_get_data(src_buf, &data);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to get data address from DOCA buffer: %s", doca_get_error_string(result));
		return result;
	}
	result = doca_buf_set_data(src_buf, data, host_offset);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set data for DOCA buffer: %s", doca_get_error_string(result));
		return result;
	}

	dma_job.src_buff = src_buf;
	dma_job.dst_buff = dst_buf;

	/* Enqueue DMA job */
	result = doca_workq_submit(workq, &dma_job.base);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to submit DMA job: %s", doca_get_error_string(result));
		return result;
	}

	/* Wait for job completion */
	while ((result = doca_workq_progress_retrieve(workq, &event, DOCA_WORKQ_RETRIEVE_FLAGS_NONE)) ==
	       DOCA_ERROR_AGAIN) {
		nanosleep(&ts, &ts);
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

	DOCA_LOG_INFO("DMA copy was done Successfully");

    ch.SendSuccessfulMsg();

	return result;
}