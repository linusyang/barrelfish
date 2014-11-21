#include <stdio.h>
#include <barrelfish/barrelfish.h>
#include <barrelfish/nameservice_client.h>
#include <barrelfish/spawn_client.h>
#include <if/idc_pingpong_defs.h>

static volatile int keep_loop = 1;

void syn_handler(struct idc_pingpong_binding *b){
	debug_printf("received syn\n");
	errval_t err = b->tx_vtbl.syn_ack(b, NOP_CONT);
    if(err_is_fail(err)){
        DEBUG_ERR(err, "send syn-ack failed");
    }
}

void syn_ack_handler(struct idc_pingpong_binding *b){
	debug_printf("received syn-ack\n");
    errval_t err = b->tx_vtbl.ack(b, NOP_CONT);
    if(err_is_fail(err)){
        DEBUG_ERR(err, "send ack failed");
    }

    /* Client finished handshaking */
    keep_loop = 0;
}

void ack_handler(struct idc_pingpong_binding *b){
	debug_printf("received ack\n");

    /* Server finished handshaking */
    keep_loop = 0;
}

/**
 * START OF MESSAGING EXPORT/BINDING SERVICE
 */

static struct idc_pingpong_rx_vtbl idc_pingpong_rx_vtbl = {
    .syn     = syn_handler,
    .syn_ack = syn_ack_handler,
    .ack     = ack_handler
};

static void run_client(struct idc_pingpong_binding *b){
    debug_printf("client sent syn\n");
    errval_t err = b->tx_vtbl.syn(b, NOP_CONT);
    if(err_is_fail(err)){
        DEBUG_ERR(err, "failed to start client");
    }
}

static void bind_cb(void *st, errval_t err, struct idc_pingpong_binding *b){
	if(err_is_fail(err)){
		USER_PANIC_ERR(err, "bind failed");
	}
	debug_printf("client bound!\n");

    // copy my message receive handler vtable to the binding
    b->rx_vtbl = idc_pingpong_rx_vtbl;

    run_client(b);
}

static void start_client(void){
	iref_t iref;
	errval_t err;

	debug_printf("client looking up '%s' in name service...\n", "idc_pingpong_service");
	err = nameservice_blocking_lookup("idc_pingpong_service", &iref);
	if(err_is_fail(err)){
		USER_PANIC_ERR(err, "nameservice_blocking_lookup failed");
	}

	debug_printf("client binding to %"PRIuIREF"...\n", iref);
	err = idc_pingpong_bind(iref, bind_cb, NULL, get_default_waitset(), IDC_BIND_FLAGS_DEFAULT);
	if(err_is_fail(err)){
		USER_PANIC_ERR(err, "bind failed");
	}
}

/* ------------------------------ SERVER ------------------------------ */

static void export_cb(void *st, errval_t err, iref_t iref){
	if(err_is_fail(err)){
		USER_PANIC_ERR(err, "export failed");
	}

	debug_printf("service exported at iref %"PRIuIREF"\n", iref);

	// register this iref with the name service
	err = nameservice_register("idc_pingpong_service", iref);
	debug_printf("service exported at iref %"PRIuIREF"\n", iref);
	if(err_is_fail(err)){
		USER_PANIC_ERR(err, "nameservice_register failed");
	}
	debug_printf("service exported at iref %"PRIuIREF"\n", iref);
}

static errval_t connect_cb(void *st, struct idc_pingpong_binding *b){
	debug_printf("service got a connection!\n");

	// copy my message receive handler vtable to the binding
	b->rx_vtbl = idc_pingpong_rx_vtbl;

	// accept the connection (we could return an error to refuse it)
	return SYS_ERR_OK;
}

static void start_server(void){
	errval_t err;

	err = idc_pingpong_export(NULL, export_cb, connect_cb, get_default_waitset(), IDC_EXPORT_FLAGS_DEFAULT);
    if(err_is_fail(err)){
    	USER_PANIC_ERR(err, "export failed");
    }
}

/**
 * END OF MESSAGING EXPORT/BINDING SERVICE
 */

int main(int argc, char* argv[]){
	debug_printf("Core ID: %d\n", disp_get_core_id());

    if(disp_get_core_id() == 0){
    	spawn_program(1, "/x86_64/sbin/idc_pingpong", argv, NULL, SPAWN_NEW_DOMAIN, NULL);

        start_server();
    }
    else start_client();

    /* The dispatch loop */
    errval_t err;
    struct waitset *ws = get_default_waitset();
    while (keep_loop){
        err = event_dispatch(ws); /* get and handle next event */
        if(err_is_fail(err)){
            DEBUG_ERR(err, "dispatch error");
            break;
        }
    }

    /* Exiting */
    if(disp_get_core_id() == 0)
        debug_printf("server exit\n");
    else
        debug_printf("client exit\n");

    return 0;
}
