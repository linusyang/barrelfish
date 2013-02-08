/*
 * Copyright (c) 2011, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef POSIXCOMPAT_H
#define POSIXCOMPAT_H

#include <sys/cdefs.h>

__BEGIN_DECLS

errval_t spawn_setup_fds(struct capref *frame, int rfd);
errval_t posixcompat_unpack_fds(void);

__END_DECLS

#endif
