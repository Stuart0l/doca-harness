#include "chan/comm_channel.h"
#include <doca_argp.h>

#include <chrono>

#include "ch_common.h"

DOCA_LOG_REGISTER(CC_SERVER::MAIN);

const char *server_name = "doca_comm_ch_server";
const int iteration = 1000000;

int main(int argc, char *argv[]) {
    using namespace doca;
    using namespace std::chrono;

    doca_error_t result;
    struct cc_config cfg;
    doca_app_mode mode = DOCA_MODE_DPU;
    char *buf;
    int64_t duration;

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
        DOCA_LOG_ERR("Failed to register Comm Channel server parameters: %s", doca_get_error_string(result));
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

    buf = new char[cfg.cc_msg_size];
    memset(buf, 0, cfg.cc_msg_size);

    auto start = high_resolution_clock::now();
    decltype(start) end;

    for (int i = 0; i < iteration; i++) {
        result = ch.SendTo(buf, cfg.cc_msg_size);
        if (result != DOCA_SUCCESS) {
            DOCA_LOG_ERR("Failed to send message: %s", doca_get_error_string(result));
            goto argp_cleanup;
        }
    }

    end = high_resolution_clock::now();
    duration = duration_cast<microseconds>(end - start).count();
    DOCA_LOG_INFO("Throughput: %f MB/s", static_cast<double>(cfg.cc_msg_size * iteration) / duration);



    delete buf;
argp_cleanup:
    doca_argp_destroy();
    
    return result;
}