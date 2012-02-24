/**
 * \file
 * \brief Publish/Subscribe client API implementation
 *
 * The individual handler functions are stored in a function table on the
 * client side. The API provides convenience functions for subscribe/
 * unsubscribe and publish.
 *
 */

/*
 * Copyright (c) 2011, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <assert.h>

#include <barrelfish/barrelfish.h>
#include <barrelfish/threads.h>
#include <if/dist2_rpcclient_defs.h>

#include <dist2/init.h>
#include <dist2/pubsub.h>

#include "common.h"
#include "handler.h"

struct subscription {
    subscription_handler_fn handler;
    const void* state;
    uint64_t server_id;
};

static struct subscription subscriber_table[MAX_SUBSCRIPTIONS] = {
        { NULL, NULL, 0 } };
static struct thread_mutex subscriber_table_lock;

static errval_t get_free_slot(subscription_t *slot)
{
    assert(slot != NULL);

    subscription_t idx = 0;
    for (; idx < MAX_SUBSCRIPTIONS; idx++) {
        if (subscriber_table[idx].handler == NULL) {
            *slot = idx;
            return SYS_ERR_OK;
        }
    }

    return DIST2_ERR_MAX_SUBSCRIPTIONS;
}

void subscribed_message_handler(struct dist2_binding *b, subscription_t id,
        char *record)
{
    thread_mutex_lock(&subscriber_table_lock);

    if (subscriber_table[id].handler != NULL) {
        subscriber_table[id].handler(id, record, (void*)subscriber_table[id].state);
    }
    else {
        fprintf(stderr, "Incoming message (%s) for subscription: %lu not delivered. "
                        "Handler not set.", record, id);
        free(record);
    }

    thread_mutex_unlock(&subscriber_table_lock);
}

/**
 * \brief Subscribe for a given type of message.
 *
 * \param[in] function Handler function in case a matching record is
 * published.
 * \param[in] state State passed on to handler function.
 * \param[out] id Id of the subscription. In case of the value is undefined.
 * \param record What type of records you want to subscribe.
 * \param ... Additional arguments to format the record using vsprintf.
 *
 * \retval SYS_ERR_OK
 * \retval DIST2_ERR_MAX_SUBSCRIPTIONS
 * \retval DIST2_ERR_PARSER_FAIL
 * \retval DIST2_ERR_ENGINE_FAIL
 */
errval_t dist_subscribe(subscription_handler_fn function, const void *state,
        subscription_t *id, const char *record, ...)
{
    assert(function != NULL);
    assert(record != NULL);
    assert(id != NULL);
    thread_mutex_lock(&subscriber_table_lock);

    va_list args;
    errval_t err = SYS_ERR_OK;

    err = get_free_slot(id);
    if (err_is_fail(err)) {
        return err;
    }

    char* buf = NULL;
    FORMAT_QUERY(record, args, buf);

    // send to skb
    struct dist2_thc_client_binding_t* cl = dist_get_thc_client();

    errval_t error_code = SYS_ERR_OK;
    uint64_t server_id = 0;
    err = cl->call_seq.subscribe(cl, buf, *id, &server_id, &error_code);
    if (err_is_ok(err)) {
        err = error_code;
    }

    if (err_is_ok(err)) {
        subscriber_table[*id] =
                (struct subscription) {.handler = function, .state = state,
            .server_id = server_id};
    }

    free(buf);
    thread_mutex_unlock(&subscriber_table_lock);
    return err;
}

/**
 * \brief Unsubscribes a subscription.
 *
 * \param id Id of the subscription (as provided by dist_subscribe).
 *
 * \retval SYS_ERR_OK
 * \retval DIST2_ERR_PARSER_FAIL
 * \retval DIST2_ERR_ENGINE_FAIL
 */
errval_t dist_unsubscribe(subscription_t id)
{
    assert(id < MAX_SUBSCRIPTIONS);
    assert(subscriber_table[id].handler != NULL);
    thread_mutex_lock(&subscriber_table_lock);

    // send to skb
    struct dist2_thc_client_binding_t* cl = dist_get_thc_client();
    errval_t error_code;
    errval_t err = cl->call_seq.unsubscribe(cl,
            subscriber_table[id].server_id, &error_code);
    if (err_is_ok(err)) {
        err = error_code;
    }

    if (err_is_ok(err)) {
        subscriber_table[id].handler = NULL;
        subscriber_table[id].state = NULL;
        subscriber_table[id].server_id = 0;
    }

    thread_mutex_unlock(&subscriber_table_lock);
    return err;
}

/**
 * \brief Publishes a record.
 *
 * \param record The record to publish.
 * \param ... Additional arguments to format the record using vsprintf.
 *
 * \retval SYS_ERR_OK
 * \retval DIST2_ERR_PARSER_FAIL
 * \retval DIST2_ERR_ENGINE_FAIL
 */
errval_t dist_publish(const char *record, ...)
{
    assert(record != NULL);

    va_list args;
    errval_t err = SYS_ERR_OK;

    char *buf = NULL;
    FORMAT_QUERY(record, args, buf);

    struct dist2_thc_client_binding_t* cl = dist_get_thc_client();
    errval_t error_code = SYS_ERR_OK;
    err = cl->call_seq.publish(cl, buf, &error_code);
    if(err_is_ok(err)) {
        err = error_code;
    }

    free(buf);
    return err;
}

/**
 * \brief Initialized publish/subscribe system.
 */
void dist_pubsub_init(void)
{
    thread_mutex_init(&subscriber_table_lock);
}
