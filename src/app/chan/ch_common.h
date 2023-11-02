#pragma once

#include <doca_dev.h>

struct cc_config {
    char cc_dev_pci_addr[DOCA_DEVINFO_PCI_ADDR_SIZE];         /* Comm Channel DOCA device PCI address */
    char cc_dev_rep_pci_addr[DOCA_DEVINFO_REP_PCI_ADDR_SIZE]; /* Comm Channel DOCA device representor PCI address */
    size_t cc_msg_size = 1024;
};

/*
 * Register the command line parameters for the DOCA CC samples
 *
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t register_cc_params(void);
