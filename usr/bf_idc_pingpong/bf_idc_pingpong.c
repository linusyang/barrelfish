#include "stdio.h"
#include "stdbool.h"                 // FOR BOOL
#include "errors/errno.h"            // FOR ERRVAL_T
#include <barrelfish/barrelfish.h>
#include "arch/x86_32/barrelfish_kpi/eflags_arch.h"
#include "arch/x86_32/barrelfish_kpi/paging_arch.h"
#include "barrelfish_kpi/syscalls.h" // FOR STRUCT SYSRET
#include "barrelfish/capabilities.h"
#include "if/idc_pingpong_defs.h"
#include "barrelfish/domain.h"
#include "barrelfish/spawn_client.h"
#include "barrelfish/waitset.h"
#include "barrelfish/nameservice_client.h"
#include "rck_dev.h"

struct idc_pingpong_binding* binding;

int FLAG_connect = 0;
int FLAG_ack     = 0;

void syn_handler(struct idc_pingpong_binding *b){
	debug_printf("arrived at syn\n");

	b->tx_vtbl.syn_ack(b, NOP_CONT);

	do{
		debug_printf("wait ack\n");

		event_dispatch(get_default_waitset());
	}while(!FLAG_ack);
}

void syn_ack_handler(struct idc_pingpong_binding *b){
	debug_printf("arrived at syn_ack\n");
}

void ack_handler(struct idc_pingpong_binding *b){
	debug_printf("arrived at ack\n");

	FLAG_ack = 1;
}

/**
 * START OF MESSAGING EXPORT/BINDING SERVICE
 */

struct idc_pingpong_rx_vtbl idc_pingpong_rx_vtbl = {
    .syn     = syn_handler,
    .syn_ack = syn_ack_handler,
    .ack     = ack_handler
};

static void bind_cb(void *st, errval_t err, struct idc_pingpong_binding *b){
	if(err_is_fail(err)){
		USER_PANIC_ERR(err, "bind failed");
	}

	debug_printf("client bound!\n");

	// copy my message receive handler vtable to the binding
	b->rx_vtbl = idc_pingpong_rx_vtbl;

	binding = b;
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

	while(binding == NULL)
		event_dispatch(get_default_waitset());
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

	FLAG_connect = 1;

	// accept the connection (we could return an error to refuse it)
	return SYS_ERR_OK;
}

static void start_server(void){
	errval_t err;

	err = idc_pingpong_export(NULL, export_cb, connect_cb, get_default_waitset(), IDC_EXPORT_FLAGS_DEFAULT);
    if(err_is_fail(err)){
    	USER_PANIC_ERR(err, "export failed");
    }

    do{
    	event_dispatch(get_default_waitset());
    }while(!FLAG_connect);
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

    if(disp_get_core_id() == 1){
    	binding->tx_vtbl.syn(binding, NOP_CONT);
    	event_dispatch(get_default_waitset());
    	binding->tx_vtbl.ack(binding, NOP_CONT);

    	debug_printf("sent ack\n");
    }
    else event_dispatch(get_default_waitset());

    return 0;
}
