/*
 * Copyright (c) 2007-2013 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */


#ifndef USB_TRANSFER_H_
#define USB_TRANSFER_H_

#include <if/usb_manager_defs.h>
#include <if/usb_manager_rpcclient_defs.h>

struct usb_device;
struct usb_xfer;
struct usb_xfer_config;

/*
 * flounder callback functions
 */
void usb_rx_transfer_setup_call(struct usb_manager_binding *bind, uint8_t type,
        usb_manager_setup_param_t params);
void usb_rx_transfer_unsetup_call(struct usb_manager_binding *bind,
        uint32_t tid);
void usb_rx_transfer_start_call(struct usb_manager_binding *bind, uint32_t tid);
void usb_rx_transfer_stop_call(struct usb_manager_binding *bind, uint32_t tid);
void usb_rx_transfer_status_call(struct usb_manager_binding *bind,
        uint32_t tid);
void usb_rx_transfer_state_call(struct usb_manager_binding *bind, uint32_t tid);
void usb_rx_transfer_clear_stall_call(struct usb_manager_binding *bind,
        uint32_t tid);



void usb_transfer_setup_ctrl_default(struct usb_device *device,
        struct usb_request_state *st);
void usb_transfer_start(struct usb_xfer *xfer);
void usb_transfer_stop(struct usb_xfer *xfer);
void usb_transfer_unsetup(struct usb_xfer **xfers, uint16_t xfer_count);
usb_error_t usb_transfer_setup(struct usb_device *device, const uint8_t *ifaces,
        struct usb_xfer **ppxfer, const struct usb_xfer_config *setup_start, uint16_t n_setup);
uint8_t usb_transfer_completed(struct usb_xfer *xfer);

#endif /* USB_TRANSFER_H_ */
