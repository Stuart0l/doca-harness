#include "device.h"

#include <doca_error.h>
#include <doca_log.h>

#include "../mem/mem.h"

namespace doca {

DOCA_LOG_REGISTER(DEVICE);

doca_error_t DOCADevice::OpenWithPci(const char* pci_addr) {
    struct doca_devinfo** dev_list;
    uint32_t nb_devs;
    uint8_t is_addr_equal = 0;
    doca_error_t res;
    size_t i;

    res = doca_devinfo_list_create(&dev_list, &nb_devs);
    if (res != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to load doca devices list. Doca_error value: %d", res);
        return res;
    }

    /* Search */
    for (i = 0; i < nb_devs; i++) {
        res = doca_devinfo_get_is_pci_addr_equal(dev_list[i], pci_addr, &is_addr_equal);
        if (res == DOCA_SUCCESS && is_addr_equal) {
            /* if device can be opened */
            res = doca_dev_open(dev_list[i], &dev);
            if (res == DOCA_SUCCESS) {
                doca_devinfo_list_destroy(dev_list);
                return res;
            }
        }
    }

    DOCA_LOG_WARN("Matching device not found");
    res = DOCA_ERROR_NOT_FOUND;

    doca_devinfo_list_destroy(dev_list);
    return res;
}

doca_error_t DOCADevice::OpenWithCap(jobs_check func) {
    struct doca_devinfo** dev_list;
    uint32_t nb_devs;
    doca_error_t result;
    size_t i;

    result = doca_devinfo_list_create(&dev_list, &nb_devs);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to load doca devices list. Doca_error value: %d", result);
        return result;
    }

    /* Search */
    for (i = 0; i < nb_devs; i++) {
        /* If any special capabilities are needed */
        if (func(dev_list[i]) != DOCA_SUCCESS) continue;

        /* If device can be opened */
        if (doca_dev_open(dev_list[i], &dev) == DOCA_SUCCESS) {
            doca_devinfo_list_destroy(dev_list);
            return DOCA_SUCCESS;
        }
    }

    DOCA_LOG_WARN("Matching device not found");
    doca_devinfo_list_destroy(dev_list);
    return DOCA_ERROR_NOT_FOUND;
}

doca_error_t DOCADevice::AddMMap(MemMap& mmap) {
    doca_error_t result;

    result = doca_mmap_dev_add(mmap.mmap, dev);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Unable to add device to mmap: %s", doca_get_error_string(result));
    }
    return result;
}

DOCADevice::~DOCADevice() {
    doca_error_t res;
    if (dev) {
        res = doca_dev_close(dev);
        if (res != DOCA_SUCCESS) DOCA_LOG_ERR("Failed to close DOCA device: %s", doca_get_error_string(res));
    }
}

doca_error_t DOCADeviceRep::OpenWithPci(DOCADevice& dev, doca_dev_rep_filter filter, const char* pci_addr) {
    uint32_t nb_rdevs = 0;
    struct doca_devinfo_rep** rep_dev_list = NULL;
    uint8_t is_addr_equal = 0;
    doca_error_t result;
    size_t i;

    /* Search */
    result = doca_devinfo_rep_list_create(dev.dev, filter, &rep_dev_list, &nb_rdevs);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR(
            "Failed to create devinfo representors list. Representor devices are available only on DPU, do not run on "
            "Host");
        return DOCA_ERROR_INVALID_VALUE;
    }

    for (i = 0; i < nb_rdevs; i++) {
        result = doca_devinfo_rep_get_is_pci_addr_equal(rep_dev_list[i], pci_addr, &is_addr_equal);
        if (result == DOCA_SUCCESS && is_addr_equal && doca_dev_rep_open(rep_dev_list[i], &dev_rep) == DOCA_SUCCESS) {
            doca_devinfo_rep_list_destroy(rep_dev_list);
            return DOCA_SUCCESS;
        }
    }

    DOCA_LOG_WARN("Matching device not found");
    doca_devinfo_rep_list_destroy(rep_dev_list);
    return DOCA_ERROR_NOT_FOUND;
}

DOCADeviceRep::~DOCADeviceRep() {
    doca_error_t res;
    if (dev_rep) {
        res = doca_dev_rep_close(dev_rep);
        if (res != DOCA_SUCCESS)
            DOCA_LOG_ERR("Failed to close DOCA device representor: %s", doca_get_error_string(res));
    }
}

}  // namespace doca
