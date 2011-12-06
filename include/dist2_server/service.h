#ifndef DIST2_SERVICE_H_
#define DIST2_SERVICE_H_

#include <barrelfish/barrelfish.h>
#include <if/dist_defs.h>
#include <if/dist_event_defs.h>

#define BUFFER_SIZE (32 * 1024)

struct dist_reply_state;

struct skb_writer {
    char buffer[BUFFER_SIZE];
    size_t length;
};

struct dist_query_state {
    struct skb_writer stdout;
    struct skb_writer stderr;
    int exec_res;
};

typedef void(*dist_reply_handler_fn)(struct dist_binding*, struct dist_reply_state*);

struct dist_reply_state {
    struct dist_binding* binding;

    struct dist_query_state query_state;
    dist_reply_handler_fn rpc_reply;
    bool return_record;
    errval_t error;

    // For watch()
    dist_binding_type_t type;
    uint64_t client_id;
    uint64_t watch_id;

    struct dist_reply_state *next;
};

void get_names_handler(struct dist_binding*, char*);
void get_handler(struct dist_binding*, char*);
void set_handler(struct dist_binding*, char*, uint64_t, bool);
void del_handler(struct dist_binding*, char*);
void exists_handler(struct dist_binding*, char*, bool);
void exists_not_handler(struct dist_binding* b, char*, bool);
void watch_handler(struct dist_binding* b, char* query, uint64_t mode, dist_binding_type_t type, uint64_t client_id);

void subscribe_handler(struct dist_binding*, char*, uint64_t);
void publish_handler(struct dist_binding*, char*);
void unsubscribe_handler(struct dist_binding*, uint64_t);

void lock_handler(struct dist_binding*, char*);
void unlock_handler(struct dist_binding*, char*);

void identify_rpc_binding(struct dist_binding*, uint64_t);
void get_identifier(struct dist_binding*);
void identify_events_binding(struct dist_event_binding*, uint64_t);

#endif /* DIST2_SERVICE_H_ */
