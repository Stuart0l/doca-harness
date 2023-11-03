#include "comm_channel.h"

#include <doca_ctx.h>
#include <doca_dev.h>

#include <stdexcept>

#define SLEEP_IN_NANOS (10 * 1000) /* Sample the job every 10 microseconds  */

namespace doca {

DOCA_LOG_REGISTER(COMM_CHANNEL);

CommChannel::CommChannel(doca_app_mode mode, const char *dev_pci_addr, const char *dev_rep_pci_addr)
    : mode(mode), connected(false) {
    doca_error_t result;

    result = doca_comm_channel_ep_create(&ep);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to create Comm Channel endpoint: %s", doca_get_error_string(result));
        throw std::runtime_error("Failed to create Comm Channel endpoint");
    }

    dev = std::make_shared<DOCADevice>();
    result = dev->OpenWithPci(dev_pci_addr);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to open Comm Channel DOCA device based on PCI address");
        dev.reset();
        throw std::runtime_error("Failed to open Comm Channel DOCA device based on PCI address");
    }

    /* Open DOCA device representor on DPU side */
    if (mode == DOCA_MODE_DPU) {
        dev_rep = std::make_shared<DOCADeviceRep>();
        result = dev_rep->OpenWithPci(*dev, DOCA_DEV_REP_FILTER_NET, dev_rep_pci_addr);
        if (result != DOCA_SUCCESS) {
            DOCA_LOG_ERR("Failed to open Comm Channel DOCA device representor based on PCI address");
            doca_comm_channel_ep_destroy(ep);
            dev.reset();
            throw std::runtime_error("Failed to open Comm Channel DOCA device representor based on PCI address");
        }
    }

    result = set_cc_properties(mode);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to set Comm Channel properties");
        doca_comm_channel_ep_destroy(ep);
        if (mode == DOCA_MODE_DPU) dev_rep.reset();
        dev.reset();
        throw std::runtime_error("Failed to set Comm Channel properties");
    }
}

CommChannel::~CommChannel() {
    doca_error_t result;
    DisConnect();

    result = doca_comm_channel_ep_destroy(ep);
    if (result != DOCA_SUCCESS)
        DOCA_LOG_ERR("Failed to destroy Comm Channel endpoint: %s", doca_get_error_string(result));

    dev_rep.reset();
    dev.reset();
}

doca_error_t CommChannel::Connect(const char *name) {
    struct timespec ts = {
        .tv_nsec = SLEEP_IN_NANOS,
    };
    doca_error_t result;
    size_t msg_len;

    result = doca_comm_channel_ep_connect(ep, name, &peer_addr);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to establish a connection with the DPU: %s", doca_get_error_string(result));
        return result;
    }

    while ((result = doca_comm_channel_peer_addr_update_info(peer_addr)) == DOCA_ERROR_CONNECTION_INPROGRESS)
        nanosleep(&ts, &ts);

    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to validate the connection with the DPU: %s", doca_get_error_string(result));
        return result;
    }

    connected = true;
    DOCA_LOG_INFO("Connection to DPU was established successfully");
    return result;
}

doca_error_t CommChannel::DisConnect() {
    if (!connected) return DOCA_SUCCESS;
    doca_error_t result;
    if (peer_addr) {
        result = doca_comm_channel_ep_disconnect(ep, peer_addr);
        if (result == DOCA_ERROR_NOT_CONNECTED) connected = false;
        if (result != DOCA_SUCCESS)
            DOCA_LOG_ERR("Failed to disconnect from Comm Channel peer address: %s", doca_get_error_string(result));
    }

    connected = false;
    return result;
}

doca_error_t CommChannel::Listen(const char *name) {
    doca_error_t result;
    result = doca_comm_channel_ep_listen(ep, name);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Comm Channel server couldn't start listening: %s", doca_get_error_string(result));
        return result;
    }

    DOCA_LOG_INFO("Server started Listening, waiting for new connections");
    return result;
}

doca_error_t CommChannel::SendTo(const void *msg, size_t len) {
    struct timespec ts = {
        .tv_nsec = SLEEP_IN_NANOS,
    };
    doca_error_t result;
    while ((result = doca_comm_channel_ep_sendto(ep, msg, len, DOCA_CC_MSG_FLAG_NONE, peer_addr)) == DOCA_ERROR_AGAIN)
        nanosleep(&ts, &ts);

    return result;
}

doca_error CommChannel::RecvFrom(void *msg, size_t *len) {
    size_t msg_len = *len;
    struct timespec ts = {
        .tv_nsec = SLEEP_IN_NANOS,
    };
    doca_error_t result;
    while ((result = doca_comm_channel_ep_recvfrom(ep, msg, &msg_len, DOCA_CC_MSG_FLAG_NONE, &peer_addr)) ==
           DOCA_ERROR_AGAIN) {
        nanosleep(&ts, &ts);
        msg_len = *len;
    }

    *len = msg_len;
    return result;
}

doca_error_t CommChannel::SendSuccessfulMsg() {
    doca_error_t result;
    struct cc_msg_status msg_status;
    size_t msg_len = sizeof(struct cc_msg_status);
    msg_status.is_success = true;

    result = SendTo(&msg_status, msg_len);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to send status message: %s", doca_get_error_string(result));
        return result;
    }

    return DOCA_SUCCESS;
}

doca_error_t CommChannel::WaitForSuccessfulMsg() {
    struct cc_msg_status msg_status;
    doca_error_t result;
    size_t msg_len = sizeof(struct cc_msg_status);

    result = RecvFrom(&msg_status, &msg_len);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Status message was not received: %s", doca_get_error_string(result));
        return result;
    }

    if (!msg_status.is_success) {
        DOCA_LOG_ERR("Failure status received");
        return DOCA_ERROR_INVALID_VALUE;
    }

    return DOCA_SUCCESS;
}

doca_error_t CommChannel::set_cc_properties(doca_app_mode mode) {
    doca_error_t result;

    result = doca_comm_channel_ep_set_device(ep, dev->dev);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to set DOCA device property");
        return result;
    }

    result = doca_comm_channel_ep_set_max_msg_size(ep, CC_MAX_MSG_SIZE);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to set max_msg_size property");
        return result;
    }

    result = doca_comm_channel_ep_set_send_queue_size(ep, CC_MAX_QUEUE_SIZE);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to set snd_queue_size property");
        return result;
    }

    result = doca_comm_channel_ep_set_recv_queue_size(ep, CC_MAX_QUEUE_SIZE);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to set rcv_queue_size property");
        return result;
    }

    if (mode == DOCA_MODE_DPU) {
        result = doca_comm_channel_ep_set_device_rep(ep, dev_rep->dev_rep);
        if (result != DOCA_SUCCESS) DOCA_LOG_ERR("Failed to set DOCA device representor property");
    }

    return result;
}

}  // namespace doca
