#include "dma_common.h"

#include <doca_argp.h>
#include <doca_error.h>
#include <doca_log.h>
#include <string.h>

DOCA_LOG_REGISTER(DMA_COMMON);

/*
 * ARGP Callback - Handle Comm Channel DOCA device PCI address paramet|er
 *
 * @param [in]: Input parameter
 * @config [in/out]: Program configuration context
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t pci_addr_callback(void *param, void *config) {
    struct dma_copy_cfg *cfg = (struct dma_copy_cfg *)config;
    const char *dev_pci_addr = (char *)param;
    int len;

    len = strnlen(dev_pci_addr, DOCA_DEVINFO_PCI_ADDR_SIZE);
    /* Check using >= to make static code analysis satisfied */
    if (len >= DOCA_DEVINFO_PCI_ADDR_SIZE) {
        DOCA_LOG_ERR("Entered device PCI address exceeding the maximum size of %d", DOCA_DEVINFO_PCI_ADDR_SIZE - 1);
        return DOCA_ERROR_INVALID_VALUE;
    }

    /* The string will be '\0' terminated due to the strnlen check above */
    strncpy(cfg->cc_dev_pci_addr, dev_pci_addr, len + 1);

    return DOCA_SUCCESS;
}

/*
 * ARGP Callback - Handle Comm Channel DOCA device representor PCI address parameter
 *
 * @param [in]: Input parameter
 * @config [in/out]: Program configuration context
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t rep_pci_addr_callback(void *param, void *config) {
    struct dma_copy_cfg *cfg = (struct dma_copy_cfg *)config;
    const char *rep_pci_addr = (char *)param;
    int len;

    len = strnlen(rep_pci_addr, DOCA_DEVINFO_PCI_ADDR_SIZE);
    /* Check using >= to make static code analysis satisfied */
    if (len >= DOCA_DEVINFO_PCI_ADDR_SIZE) {
        DOCA_LOG_ERR("Entered device representor PCI address exceeding the maximum size of %d",
                     DOCA_DEVINFO_PCI_ADDR_SIZE - 1);
        return DOCA_ERROR_INVALID_VALUE;
    }

    /* The string will be '\0' terminated due to the strnlen check above */
    strncpy(cfg->cc_dev_rep_pci_addr, rep_pci_addr, len + 1);

    return DOCA_SUCCESS;
}

doca_error_t msg_size_callback(void *param, void *config) {
    struct dma_copy_cfg *cfg = (struct dma_copy_cfg *)config;

    cfg->chunk_size = *(int *)param;

    return DOCA_SUCCESS;
}

doca_error_t register_dma_copy_params(void) {
    doca_error_t result;
    struct doca_argp_param *chunk_size_param, *dev_pci_addr_param, *rep_pci_addr_param;

    /* Create and register Comm Channel DOCA device PCI address */
    result = doca_argp_param_create(&dev_pci_addr_param);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
        return result;
    }
    doca_argp_param_set_short_name(dev_pci_addr_param, "p");
    doca_argp_param_set_long_name(dev_pci_addr_param, "pci-addr");
    doca_argp_param_set_description(dev_pci_addr_param, "DOCA Comm Channel device PCI address");
    doca_argp_param_set_callback(dev_pci_addr_param, pci_addr_callback);
    doca_argp_param_set_type(dev_pci_addr_param, DOCA_ARGP_TYPE_STRING);
    doca_argp_param_set_mandatory(dev_pci_addr_param);
    result = doca_argp_register_param(dev_pci_addr_param);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
        return result;
    }

    /* Create and register Comm Channel DOCA device representor PCI address */
    result = doca_argp_param_create(&rep_pci_addr_param);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
        return result;
    }
    doca_argp_param_set_short_name(rep_pci_addr_param, "r");
    doca_argp_param_set_long_name(rep_pci_addr_param, "rep-pci");
    doca_argp_param_set_description(rep_pci_addr_param,
                                    "DOCA Comm Channel device representor PCI address (needed only on DPU)");
    doca_argp_param_set_callback(rep_pci_addr_param, rep_pci_addr_callback);
    doca_argp_param_set_type(rep_pci_addr_param, DOCA_ARGP_TYPE_STRING);
    result = doca_argp_register_param(rep_pci_addr_param);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
        return result;
    }

    /* Create and register DMA copy data chunk size */
    result = doca_argp_param_create(&chunk_size_param);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
        return result;
    }
    doca_argp_param_set_short_name(chunk_size_param, "s");
    doca_argp_param_set_long_name(chunk_size_param, "chunk-size");
    doca_argp_param_set_description(chunk_size_param, "DOCA DMA copy chunk size");
    doca_argp_param_set_callback(chunk_size_param, msg_size_callback);
    doca_argp_param_set_type(chunk_size_param, DOCA_ARGP_TYPE_INT);
    result = doca_argp_register_param(chunk_size_param);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
        return result;
    }

    return DOCA_SUCCESS;
}
