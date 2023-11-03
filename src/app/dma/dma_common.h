#pragma once

#include <doca_dev.h>

struct dma_copy_cfg {
    char cc_dev_pci_addr[DOCA_DEVINFO_PCI_ADDR_SIZE];         /* Comm Channel DOCA device PCI address */
    char cc_dev_rep_pci_addr[DOCA_DEVINFO_REP_PCI_ADDR_SIZE]; /* Comm Channel DOCA device representor PCI address */
    uint32_t chunk_size;                                      /* Chunk size in bytes */
};

/*
 * Register application arguments
 *
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t register_dma_copy_params(void);
