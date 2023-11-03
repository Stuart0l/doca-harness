#include <doca_argp.h>



#include "ch_common.h"
#include "chan/comm_channel.h"
#include "dev/device.h"

DOCA_LOG_REGISTER(CC_CLIENT::MAIN);

const char *server_name = "doca_comm_ch_server";
const int iteration = 1000000;

int main(int argc, char *argv[]) {
    using namespace doca;

    doca_error_t result;
    struct cc_config cfg;
    doca_app_mode mode = DOCA_MODE_HOST;
    char *buf;
    size_t msg_len;

    result = doca_log_create_standard_backend();
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to create standard log backend");
        return result;
    }

    result = doca_argp_init("doca_comm_ch_client", &cfg);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to init ARGP resources: %s", doca_get_error_string(result));
        return result;
    }

    result = register_cc_params();
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to register Comm Channel client parameters: %s", doca_get_error_string(result));
        doca_argp_destroy();
        return result;
    }

    result = doca_argp_start(argc, argv);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to parse sample input: %s", doca_get_error_string(result));
        doca_argp_destroy();
        return result;
    }

    CommChannel ch(mode, cfg.cc_dev_pci_addr, cfg.cc_dev_rep_pci_addr);

    result = ch.Connect(server_name);
    if (result != DOCA_SUCCESS) {
        goto argp_cleanup;
    }
    result = ch.SendSuccessfulMsg();
    if (result != DOCA_SUCCESS) {
        goto argp_cleanup;
    }

    buf = new char[cfg.cc_msg_size];
    memset(buf, 42, cfg.cc_msg_size);

    for (int i = 0; i < iteration; i++) {
        msg_len = cfg.cc_msg_size;
        result = ch.RecvFrom(buf, &msg_len);
        if (result != DOCA_SUCCESS) {
            DOCA_LOG_ERR("Failed to receive message :%s", doca_get_error_string(result));
        }
    }


    ch.DisConnect();
    delete buf;
argp_cleanup:
    doca_argp_destroy();

    return result;
}
