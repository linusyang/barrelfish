/*
 * Copyright (c) 2007, 2008, 2009, 2011, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */


#include <stdio.h>
#include <unistd.h>

#include <barrelfish/barrelfish.h>
#include <vfs/vfs.h>
#include <sys/types.h>
#include <eclipse.h>
#include <include/skb_server.h>
#include <include/skb_debug.h>

#include <dist2_server/init.h>


#include "dist/predicates.h"
#include "shared_lib_dict.h"

#define MEMORY_SIZE 32*1024*1024
#define ECLIPSE_DIR "/skb"

#define RESULT_BUF_SIZE 1024

int skb_init(void);
void execute_string(char *string);

/*
static void test(void)
{
    dident  p_1,fail;
    ec_ref X,Start;
    long num;

    ec_init();

    // make a set of facts
    ec_exec_string("compile_term([p(1),p(2),p(3),p(2),p(1)])",0);

    // make atoms and functors
    p_1 = ec_did("p",1);
    fail = ec_did("fail",0);

    // we will call p(X) and get an instantiation
    X = ec_ref_create_newvar();

    // Start will contain choice point before executing p(X)
    Start = ec_ref_create(ec_nil());

    // Fail loop
    ec_post_goal(ec_term(p_1,ec_ref_get(X)));
    while(PSUCCEED == ec_resume1(Start))
    {
    // on each iteration X is instantiated to a different number
        if (PSUCCEED == ec_get_long(ec_ref_get(X),&num))
    {
        printf("p(%d)\n", (int)num);

        // at 3 we want to exit the loop, cutting away other choices
        if(num == 3)
        ec_cut_to_chp(Start);
    }

    printf("ec_post_goal!\n");
    ec_post_goal(ec_atom(fail));
    }

    printf("done!\n");
    ec_ref_destroy(X);
    ec_ref_destroy(Start);

    ec_cleanup();
    exit(0);
}
*/

int main(int argc, char**argv)
{
    // we'll be needing this...
    vfs_mkdir("/tmp");

	// make sure, that dlsym has the right table to the statically compiled-in
	// shared libraries...
    dlopen_set_params(&funcs, sizeof(funcs)/sizeof(struct function_entry));

	// now set the right values for the eclipse-clp engine
    ec_set_option_int(EC_OPTION_IO, MEMORY_IO);
    ec_set_option_ptr(EC_OPTION_ECLIPSEDIR, ECLIPSE_DIR);
    ec_set_option_long(EC_OPTION_GLOBALSIZE, MEMORY_SIZE);

	// ec_.m.vm_flags |= 8;
    int n = ec_init();
    if (n != 0) {
        SKB_DEBUG("\nskb_main: ec_init() failed. Return code = %d\n", n);
    } else {
        SKB_DEBUG("\nskb_main: ec_init() succeeded.\n");
    }

    /*ec_ref Start = ec_ref_create_newvar();
    dident fail = ec_did("fail",0);
    ec_post_goal(ec_atom(fail));
    assert(ec_resume1(Start) == PFAIL);
    ec_ref_destroy(Start);
    assert(!"worked");*/

    execute_string("set_flag(print_depth,100).");

    errval_t err = dist_server_init();
    assert(err_is_ok(err));

    skb_server_init();
    SKB_DEBUG("\nskb initialized\n");

    chdir(ECLIPSE_DIR);


    // dist2 related stuff
    execute_string("[objects].");
    execute_string("[pubsub].");
    execute_string("[bindings].");

    ec_external(ec_did("notify_client", 2), p_notify_client, ec_did("eclipse",0));
    ec_external(ec_did("trigger_watch", 5), p_trigger_watch, ec_did("eclipse",0));
    ec_external(ec_did("identification_complete", 1), p_identification_complete, ec_did("eclipse",0));

    // regex hack TODO
    ec_external(ec_did("match", 3), (int (*)()) ec_regmatch, ec_did("eclipse",0));
    ec_external(ec_did("split", 4), (int (*)()) ec_regsplit, ec_did("eclipse",0));
    // end

    // SKB Hardware related
    execute_string("[queries].");
    // execute_string("get_local_affinity(1,B,L),write(output,[B,L]).");

    // execute_string("lib(branch_and_bound).");
    // execute_string("minimize(member(X,[4,1,2]),X),write(output,X).");


    while (1) {
        messages_wait_and_handle_next();

        /*
        ec_ref Start = ec_ref_create_newvar();
        dident fail = ec_did("fail",0);
        ec_post_goal(ec_atom(fail));
        int res = ec_resume1(Start);
        assert(res == PFAIL);
        ec_ref_destroy(Start);
        */
    }
}


int skb_init(void)
{
    int n;

    SKB_DEBUG("\ninitialize eclipse\n");
    n = ec_init();

    if (n != 0) {
        SKB_DEBUG("\nskb_main: ec_init() failed.");
    }
    return (0);
}


void execute_string(char *string)
{
    char    buf[RESULT_BUF_SIZE];
    int n;

    ec_post_string(string);
    int res = 7; //means that we have to flush the output.
    while (res == 7) {
        res = ec_resume();
        SKB_DEBUG("\nres = %d\n", res);

        //give back the result and the error messages.
        //in case there is no message, a '.' is still returned
        n = ec_queue_read(1, buf, RESULT_BUF_SIZE);
        if ((n >=0) && (n < RESULT_BUF_SIZE)) {
            buf[n] = 0;
        }
        SKB_DEBUG("eclipse returned: %s with length %d.\n", buf,n);

        n = ec_queue_read(2, buf, RESULT_BUF_SIZE);
        if ((n >=0) && (n < RESULT_BUF_SIZE)) {
            buf[n] = 0;
        }
        SKB_DEBUG("eclipse error returned: %s with length %d.\n", buf,n);
    }
}
