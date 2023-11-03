#include "mem.h"

#include <doca_error.h>
#include <doca_log.h>

#include <stdexcept>

#include "../dma/dma.h"

namespace doca {

DOCA_LOG_REGISTER(MEM_REGION);

MemMap::MemMap() : mode(MMAP_MODE_LOCAL) {
    doca_error_t result;
    result = doca_mmap_create(nullptr, &mmap);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to create mmap: %s", doca_get_error_string(result));
        throw std::runtime_error("Unable to create mmap");
    }
}

MemMap::MemMap(DOCADma& dma, CommChannel& ch) : mode(MMAP_MODE_REMOTE) {
    doca_error_t result;
    result = RecvDesc(ch);
    if (result != DOCA_SUCCESS) throw std::runtime_error("Failed to receive descriptor");
    /* Create a local DOCA mmap from export descriptor */
    result = doca_mmap_create_from_export(NULL, (const void*)export_desc.desc, export_desc.len, dma.dev->dev, &mmap);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to create memory map from export descriptor");
        throw std::runtime_error("Failed to create memory map from export descriptor");
    }
}

MemMap::~MemMap() {
    doca_error_t result;
    if (mmap) {
        result = doca_mmap_destroy(mmap);
        if (result != DOCA_SUCCESS) DOCA_LOG_ERR("Failed to destroy mmap: %s", doca_get_error_string(result));
        mmap = NULL;
    }

    if (buffer && mode == MMAP_MODE_LOCAL) delete buffer;
}

doca_error_t MemMap::AllocAndPopulate(uint32_t access_flags, size_t buffer_len) {
    doca_error_t result;

    result = doca_mmap_set_permissions(mmap, access_flags);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to set access permissions of memory map: %s", doca_get_error_string(result));
        return result;
    }

    buffer = new char[buffer_len];
    len = buffer_len;
    if (buffer) {
        DOCA_LOG_ERR("Failed to allocate memory for source buffer");
        return DOCA_ERROR_NO_MEMORY;
    }

    result = doca_mmap_set_memrange(mmap, buffer, buffer_len);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to set memrange of memory map: %s", doca_get_error_string(result));
        delete buffer;
        len = 0;
        return result;
    }

    /* Populate local buffer into memory map to allow access from DPU side after exporting */
    result = doca_mmap_start(mmap);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to populate memory map: %s", doca_get_error_string(result));
        len = 0;
        delete buffer;
    }

    return result;
}

doca_error_t MemMap::ExportDPU(DOCADevice& dev) {
    doca_error_t result;

    /* Export memory map to allow access to this memory region from DPU */
    result = doca_mmap_export_dpu(mmap, dev.dev, &export_desc.desc, &export_desc.len);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to export DOCA mmap: %s", doca_get_error_string(result));
    }
    return result;
}

doca_error_t MemMap::SendDesc(CommChannel& ch) {
    doca_error_t result;
    result = ch.SendTo(export_desc.desc, export_desc.len);

    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to send config files to DPU: %s", doca_get_error_string(result));
        return result;
    }

    result = ch.WaitForSuccessfulMsg();
    if (result != DOCA_SUCCESS) return result;

    return DOCA_SUCCESS;
}

doca_error_t MemMap::RecvDesc(CommChannel& ch) {
    doca_error_t result;
    size_t msg_len;

    msg_len = CC_MAX_MSG_SIZE;
    result = ch.RecvFrom(&export_desc, &msg_len);

    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to receive export descriptor from Host: %s", doca_get_error_string(result));
        ch.SendFailMsg();
        return result;
    }

    return result;
}

doca_error_t MemMap::SendAddrAndOffset(CommChannel& ch) {
    doca_error_t result;

    uint64_t addr_to_send = (uintptr_t)buffer;

    result = ch.SendTo(&addr_to_send, sizeof(addr_to_send));
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to send address to start DMA from: %s", doca_get_error_string(result));
        return result;
    }

    result = ch.WaitForSuccessfulMsg();
    if (result != DOCA_SUCCESS) return result;

    result = ch.SendTo(&len, sizeof(len));
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to send buffer size to DPU: %s", doca_get_error_string(result));
        return result;
    }
    result = ch.WaitForSuccessfulMsg();
    if (result != DOCA_SUCCESS) return result;

    DOCA_LOG_INFO("Address and offset to start DMA from sent successfully, waiting for DPU to Ack that DMA finished");

    return result;
}

doca_error_t MemMap::RecvAddrAndOffset(CommChannel& ch) {
    doca_error_t result;
    uint64_t received_addr, received_addr_len;
    size_t msg_len;

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
    buffer = (char*)received_addr;

    DOCA_DLOG_INFO("Remote address received successfully from Host: %" PRIu64 "", received_addr);
    result = ch.SendSuccessfulMsg();
    if (result != DOCA_SUCCESS) return result;

    msg_len = sizeof(received_addr_len);
    result = ch.RecvFrom(&received_addr_len, &msg_len);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to receive remote address offset from Host: %s", doca_get_error_string(result));
        ch.SendFailMsg();
        return result;
    }

    if (received_addr_len > SIZE_MAX) {
        DOCA_LOG_ERR("Offset exceeds SIZE_MAX in this device");
        ch.SendFailMsg();
        return DOCA_ERROR_INVALID_VALUE;
    }
    len = (size_t)received_addr_len;

    DOCA_DLOG_INFO("Address offset received successfully from Host: %" PRIu64 "", received_addr_len);

    result = ch.SendSuccessfulMsg();
    if (result != DOCA_SUCCESS) return result;

    return result;
}

}  // namespace doca
