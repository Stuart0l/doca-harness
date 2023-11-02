#pragma once

#include <doca_comm_channel.h>
#include <doca_error.h>
#include <doca_log.h>

#include <memory>

#include "../common.h"
#include "../dev/device.h"

#define MAX_ARG_SIZE 128               /* PCI address and file path maximum length */
#define MAX_DMA_BUF_SIZE (1024 * 1024) /* DMA buffer maximum size */
#define CC_MAX_MSG_SIZE 4080           /* Comm Channel message maximum size */
#define CC_MAX_QUEUE_SIZE 10           /* Max number of messages on Comm Channel queue */

namespace doca {

struct cc_msg_status {
    bool is_success;
};

class CommChannel {
   public:
    CommChannel(doca_app_mode mode, const char *dev_pci_addr, const char *dev_rep_pci_addr);
    ~CommChannel();

    doca_error_t Connect(const char *name);
    doca_error_t DisConnect();
    doca_error_t Listen(const char *name);
    doca_error_t SendTo(const void *msg, size_t len);
    doca_error RecvFrom(void *msg, size_t *len);
    doca_error_t SendSuccessfulMsg();
    doca_error_t WaitForSuccessfulMsg();

   protected:
    struct doca_comm_channel_ep_t *ep;
    struct doca_comm_channel_addr_t *peer_addr;
    std::shared_ptr<DOCADevice> dev;
    std::shared_ptr<DOCADeviceRep> dev_rep;
    doca_app_mode mode;

    doca_error_t set_cc_properties(doca_app_mode mode);
};

}  // namespace doca
