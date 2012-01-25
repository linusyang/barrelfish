/**
 * \file
 * \brief
 */

/*
 * Copyright (c) 2007, 2008, 2010, 2011, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdio.h>
#include <string.h>
#include <errors/errno.h>
#include <barrelfish/types.h>
#include <barrelfish_kpi/syscalls.h>
#include <capabilities.h>
#include <cap_predicates.h>
#include <mdb/mdb.h>
#include <mdb/mdb_tree.h>

void set_cap_remote(struct cte *cte, bool is_remote) 
{
    assert(cte != NULL);

    cte->mdbnode.remote_relations = is_remote;   // set is_remote on this cte

    struct cte *next = mdb_successor(cte);

    // find all relations and set is_remote on them
    while (next) {
        if (is_copy(&next->cap, &cte->cap) || is_ancestor(&next->cap, &cte->cap)
            || is_ancestor(&cte->cap, &next->cap)) {
            next->mdbnode.remote_relations = is_remote;
        } else {
            break;
        }
        next = mdb_successor(next);
    }
}

bool is_cap_remote(struct cte *cte) 
{
    return cte->mdbnode.remote_relations;
}

/// Check if #cte has any descendants
/// XXX TODO - check for descendents on remote cores
bool has_descendants(struct cte *cte)
{
    assert(cte != NULL);

    struct cte *next = mdb_successor(cte);

    while (next) {
        if (is_ancestor(&next->cap, &cte->cap)) {
            return true;
        }
        else if (!is_copy(&next->cap, &cte->cap)) {
            break;
        }
        next = mdb_successor(next);
    }

    return false;
}

/// Check if #cte has any ancestors
/// XXX TODO - check for descendents on remote cores
bool has_ancestors(struct cte *cte)
{
    assert(cte != NULL);

    struct cte *prev = mdb_predecessor(cte);

    while(prev) {
        if (is_ancestor(&cte->cap, &prev->cap)) {
            return true;
        }
        else if (!is_copy(&prev->cap, &cte->cap)) {
            break;
        }
        prev = mdb_predecessor(prev);
    }

    return false;
}

/// Checks if #cte has any copies
bool has_copies(struct cte *cte)
{
    assert(cte != NULL);

    struct cte *next = mdb_successor(cte);
    while(next) {
        if (is_copy(&next->cap, &cte->cap)) {
            return true;
        }
        if (!is_ancestor(&next->cap, &cte->cap)) {
            break;
        }
        next = mdb_successor(next);
    }

    struct cte *prev = mdb_predecessor(cte);
    while(prev) {
        if (is_copy(&prev->cap, &cte->cap)) {
            return true;
        }
        if (!is_ancestor(&prev->cap, &cte->cap)) {
            break;
        }
        prev = mdb_predecessor(prev);
    }

    return false;
}

/**
 * \brief Returns a copy of the #cap
 */
errval_t mdb_get_copy(struct capability *cap, struct capability **ret)
{
    assert(cap != NULL);
    assert(ret != NULL);

    struct cte *cte = mdb_find_equal(cap);
    if (cte) {
        *ret = &cte->cap;
        return SYS_ERR_OK;
    }
    else {
        return SYS_ERR_NO_LOCAL_COPIES;
    }
}

bool mdb_is_sane(void)
{
    if (mdb_check_invariants() != 0) {
        return false;
    }

    // TODO: check following conditon on each element of mdb
    //if ((lvaddr_t)walk < BASE_PAGE_SIZE || walk->cap.type == ObjType_Null) {
    //    return false;
    //}

    return true;
}

/**
 * Place #dest_start in the mapping database in the appropriate location.
 *
 * Look for its relations: copies and descendants and place in accordance.
 * If no relations found, place at the top and set map_head to point to it.
 */
void set_init_mapping(struct cte *dest_start, size_t num)
{
    for (size_t i = 0; i < num; i++) {
        mdb_insert(&dest_start[i]);
    }
}

/// Remove one cap from the mapping database
void remove_mapping(struct cte *cte)
{
    mdb_remove(cte);
}

