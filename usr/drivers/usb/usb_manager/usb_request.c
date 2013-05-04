/*
 * Copyright (c) 2007-2013 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdlib.h>
#include <stdio.h>
#include <barrelfish/barrelfish.h>

#include <if/usb_manager_defs.h>
#include <if/usb_manager_rpcclient_defs.h>

#include <usb/usb.h>
#include <usb/usb_request.h>
#include <usb/usb_device.h>
#include <usb/usb_xfer.h>

#include "usb_controller.h"
#include "usb_request.h"
#include "usb_transfer.h"
#include "usb_memory.h"

static void free_request_state(struct usb_request_state *st)
{
    /*
     * todo: Free XFER
     */
    free(st);
}

static void usb_request_send_callback(void *a)
{
    debug_printf("request reply send successfully");
    struct usb_request_state *st = (struct usb_request_state *) a;

    free_request_state(st);
}

static void usb_tx_request_write_response(void *a)
{
    errval_t err;
    struct usb_request_state *st = (struct usb_request_state *) a;

    debug_printf("send reply to request_write()\n");

    struct event_closure txcont = MKCONT(usb_request_send_callback, st);

    err = usb_manager_request_write_response__tx(st->binding, txcont,
            st->status);
}

static void usb_tx_request_response(void *a)
{
    errval_t err;
    struct usb_request_state *st = (struct usb_request_state *) a;

    debug_printf("send reply to request()\n");

    struct event_closure txcont = MKCONT(usb_request_send_callback, st);

    err = usb_manager_request_response__tx(st->binding, txcont, st->status);

}

static void usb_tx_request_read_response(void *a)
{
    errval_t err;
    struct usb_request_state *st = (struct usb_request_state *) a;

    debug_printf("send reply to request_read()\n");

    struct event_closure txcont = MKCONT(usb_request_send_callback, st);

    void *data = NULL;
    uint16_t size = 0;

    err = usb_manager_request_read_response__tx(st->binding, txcont, data, size,
            st->status);

}

static void usb_request_send_error(usb_error_t err,
        struct usb_manager_binding *b, void (*callback)(void *a))
{
    struct usb_request_state *rs = malloc(sizeof(struct usb_request_state));

    rs->xfer = NULL;
    rs->binding = b;
    callback(rs);
}

void usb_rx_request_read_call(struct usb_manager_binding *binding,
        uint8_t *request, size_t req_length)
{
    debug_printf("server: received request_read()\n");

    // check if we have received the correct amount of data
    if (req_length != sizeof(struct usb_device_request)) {
        debug_printf("received too less data to fullfill the request:\n "
                "request length: expected %i bytes, was %i\n",
                sizeof(struct usb_device_request), req_length);
        usb_request_send_error(USB_ERR_INVAL, binding,
                usb_tx_request_read_response);
    }

    /*
     * execute request and prepare reply
     */
    struct usb_request_state *st = malloc(sizeof(struct usb_request_state));

    struct usb_device *device = (struct usb_device *) binding->st;
    struct usb_device_request *req = (struct usb_device_request *) request;

    st->binding = binding;
    st->data_length = 0;
    st->data = malloc(req->wLength);
    st->callback = usb_tx_request_read_response;

    usb_handle_request(device, 0, req, st, st->data, &st->data_length);
}

void usb_rx_request_write_call(struct usb_manager_binding *binding,
        uint8_t *request, size_t req_length, uint8_t *data, size_t data_length)
{
    debug_printf("server: received request_write() of %i bytes\n", data_length);

    struct usb_device_request *req = (struct usb_device_request *) request;

    // check if we have received the correct amount of data
    if ((req_length != sizeof(struct usb_device_request))
            || (req->wLength != data_length)) {
        debug_printf("received too less data to fullfill the request:\n "
                "request length: expected %i bytes, was %i\n"
                "data length: expected %i bytes, was %i\n",
                sizeof(struct usb_device_request), req_length, req->wLength,
                data_length);
        usb_request_send_error(USB_ERR_INVAL, binding,
                usb_tx_request_write_response);
    }

    /*
     * execute request and prepare reply
     */
    struct usb_request_state *st = malloc(sizeof(struct usb_request_state));
    st->binding = binding;
    // write requests have no data to return
    st->data_length = 0;
    st->data = NULL;
    st->callback = usb_tx_request_write_response;

    struct usb_device *device = (struct usb_device *) binding->st;

    usb_handle_request(device, 0, req, st, st->data, &st->data_length);

}

void usb_rx_request_call(struct usb_manager_binding *binding, uint8_t *request,
        size_t req_length)
{
    debug_printf("server: received request()\n");

    // check if we have received the correct amount of data
    if (req_length != sizeof(struct usb_device_request)) {
        debug_printf("received too less data to fullfill the request:\n "
                "request length: expected %i bytes, was %i\n",
                sizeof(struct usb_device_request), req_length);
        usb_request_send_error(USB_ERR_INVAL, binding, usb_tx_request_response);
    }

    /*
     * execute request and prepare reply
     */
    struct usb_request_state *st = malloc(sizeof(struct usb_request_state));
    st->binding = binding;
    st->data_length = 0;
    st->data = NULL;
    st->callback = usb_tx_request_response;

    struct usb_device *device = (struct usb_device *) binding->st;
    struct usb_device_request *req = (struct usb_device_request *) request;

    usb_handle_request(device, 0, req, st, st->data, &st->data_length);

}

/**
 * \brief   this function handles the USB requests
 */
usb_error_t usb_handle_request(struct usb_device *device, uint16_t flags,
        struct usb_device_request *req, struct usb_request_state *req_state,
        void *data, uint16_t *ret_length)
{
    struct usb_xfer *xfer;
    usb_error_t err;
    uint16_t length = req->wLength;
    uint16_t actual_length = 0;

    /*
     * check if the device is in the correct state to handle requests
     * the device must be at lease in the powered state
     */
    if (device->state < USB_DEVICE_STATE_POWERED) {
        debug_printf("Error: USB Device has not been configured\n");
        if (req_state) {
            req_state->status = USB_ERR_NOT_CONFIGURED;
            req_state->callback(req_state->binding);
        }
        return USB_ERR_NOT_CONFIGURED;
    }

    /*
     * reset the length value
     */
    if (ret_length) {
        *ret_length = 0;
    }

    /*
     * the device may be the root hub, so we need to take the root hub
     * execution function for this, the root hub is the device which
     * does not have a parent hub associated
     */
    if (device->parent_hub == NULL) {

        /*
         * cannot write data to the root hub
         */
        if ((req->bType.direction != USB_DIRECTION_D2H) && (length != 0)) {
            debug_printf("Error: root hub does not support writing of data\n");
            if (req_state) {
                req_state->status = USB_ERR_INVAL;
                req_state->callback(req_state->binding);
            }
            return USB_ERR_INVAL;
        }
        const void *ret_desc;
        uint16_t ret_size;
        err = device->controller->hcdi_bus_fn->roothub_exec(device, req,
                &ret_desc, &ret_size);

        if (err != USB_ERR_OK) {
            if (req_state) {
                req_state->status = err;
                req_state->callback(req_state->binding);
            }

            return err;
        }

        /*
         * we have encountered a short transfer, this may be ok when the flag
         * is set
         */
        if (length > ret_size) {
            if (!(flags & USB_REQUEST_FLAG_IGNORE_SHORT_XFER)) {
                if (req_state) {
                    req_state->status = USB_ERR_SHORT_XFER;
                    req_state->callback(req_state->binding);
                }
                return USB_ERR_SHORT_XFER;
            }

            // short xfers are ok so update the length
            length = ret_size;
        }
        if (ret_length) {
            *ret_length = length;
        }

        /*
         * we have some data that we have to return
         */
        if (length > 0 && data != NULL) {
            memcpy(data, ret_desc, length);
        }

        req_state->status = USB_ERR_OK;

        return USB_ERR_OK;
    }

    /*
     * we are executing the request on a real device so we have to setup
     * a new USB control transfer on this device
     */

    usb_transfer_setup_ctrl_default(device, req_state);

    xfer = device->ctrl_xfer[0];

    if (xfer == NULL) {
        return USB_ERR_NOMEM;
    }

    xfer->usb_manager_request_callback = req_state->callback;
    xfer->usb_manager_request_state = req_state;
    req_state->xfer = xfer;

    /*
     * we have a xfer so set it up according to the setup
     * and the given flags
     */
    if (flags & USB_REQUEST_FLAG_DELAY_STATUS) {
        xfer->flags.manual_status = 1;
    } else {
        xfer->flags.manual_status = 0;
    }

    if (flags & USB_REQUEST_FLAG_IGNORE_SHORT_XFER) {
        xfer->flags.short_xfer_ok = 1;
    } else {
        xfer->flags.short_xfer_ok = 1;
    }

    xfer->timeout = 1000;   // TODO: TIMEOUT

    /*
     * copy the request into DMA memory
     */
    usb_mem_copy_in(xfer->frame_buffers, 0, req, sizeof(*req));
    xfer->frame_lengths[0] = sizeof(*req);

    /*
     * loop till we got all requested data
     */
    uint16_t current_data_length;
    while (1) {
        current_data_length = length;
        if (current_data_length > xfer->max_data_length) {
            current_data_length = xfer->max_data_length;
        }
        // set the frame length of the data stage
        xfer->frame_lengths[1] = current_data_length;

        /*
         * we have a data stage, so we have to handle the data read or write
         * in the case of data write, we have to copy the data into the
         * second frame buffer and indicate that we have two frames
         */
        if (current_data_length > 0) {
            if (!(req->bType.direction = USB_DIRECTION_D2H)) {
                usb_mem_copy_in(xfer->frame_buffers + 1, 0, data,
                        current_data_length);
            }
            xfer->num_frames = 2;
        } else {
            xfer->num_frames = 1;
        }

        usb_transfer_start(xfer);

        while (!usb_transfer_completed(xfer)) {
            /*
             * TODO: WAIT
             */
            thread_yield();
        }

        /*
         * transfer is complete, check for error condition
         */
        err = xfer->error;

        if (err != USB_ERR_OK) {
            break;
        }

        /*
         * get the actual number of frames
         */
        if (xfer->actual_frames < 2) {
            actual_length = 0;  // no data stage
        } else {
            actual_length = xfer->frame_lengths[1];
        }

        /*
         * updating variables to catch short packets
         */
        if (current_data_length > actual_length) {
            current_data_length = actual_length;
            length = current_data_length;
        }

        /*
         * copy the data out to buffer if it is a read request
         * and we have some bytes to read
         */
        if ((current_data_length > 0)
                && (req->bType.direction == USB_DIRECTION_D2H)) {
            usb_mem_copy_out(xfer->frame_buffers + 1, 0, data,
                    current_data_length);
        }

        /*
         * update the frame length accordingly
         */
        xfer->frame_lengths[0] = 0;
        length -= current_data_length;

        /*
         * advance buffer pointer
         */
        data += current_data_length;

        if (ret_length) {
            (*ret_length) += current_data_length;
        }

        /*
         * TODO: Timeout
         */

    }

    if (err != USB_ERR_OK) {
        usb_transfer_stop(xfer);
    }

    if (req_state) {
        req_state->status = err;
        req_state->callback(req_state->binding);
    }

    return (usb_error_t) err;
}
