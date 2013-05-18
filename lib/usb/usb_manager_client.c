/*
 * Copyright (c) 2007-2013 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdio.h>
#include <string.h>

#include <barrelfish/barrelfish.h>
#include <barrelfish/nameservice_client.h>

#include <usb/usb.h>
#include <usb/usb_error.h>
#include <usb/usb_driver.h>

#include <if/usb_manager_defs.h>
#include <if/usb_manager_rpcclient_defs.h>

#include "usb_manager_client.h"

#if 0
static void done_cb(struct usb_manager_binding *_binding, uint32_t tid, uint32_t error, uint8_t *data, size_t length)
{
    printf("received done notify!");
}
#endif

/*
 * --------------------------------------------------------------------------
 * Variables for connection management to the USB manager service
 * --------------------------------------------------------------------------
 */

/// the iref of the usb manager for connections
iref_t usb_manager_iref;

/// the usb manager RPC client structure
struct usb_manager_rpc_client usb_manager;

/// string representing the usb manager service identifier
static const char *usb_manager_name = "usb_manager_service";

//static struct usb_manager_rx_vtbl driver_fn = {
//        .transfer_done_notify = done_cb
//};



/**
 *
 */
static void usb_bind_cb(void *st, errval_t err,
        struct usb_manager_binding *b)
{
    debug_printf("libusb: bind callback complete\n");

    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "USB manager binding failed");
    }

    usb_manager_rpc_client_init(&usb_manager, b);

    //b->rx_vtbl.transfer_done_notify = done_cb;

    //b->rx_vtbl.transfer_done_notify_response = done_cb;

    uint32_t ret_status;

    printf("about to call...");

    err=usb_manager.vtbl.connect(&usb_manager, 0, &ret_status);

    debug_printf("libusb: driver connected (status=%i)", ret_status);

}

/**
 * \brief   does the initialization of the USB library and the binding to the
 *          USB manager service
 *
 */
usb_error_t usb_driver_init(void)
{
    errval_t err;

    debug_printf("libusb: initialization.\n");

    err = nameservice_blocking_lookup(usb_manager_name, &usb_manager_iref);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "USB manager service lookup failed");
    }

    err = usb_manager_bind(usb_manager_iref, usb_bind_cb,
            NULL /* state for bind_cb */, get_default_waitset(),
            IDC_BIND_FLAGS_DEFAULT);

    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "USB manager binding failed");
    }



    return (USB_ERR_OK);
}
