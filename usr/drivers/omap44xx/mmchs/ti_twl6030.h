/*
 * Copyright (c) 2012, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, CAB F.78, Universitaetstr 6, CH-8092 Zurich.
 */
#ifndef __TI_TWL6030_H__
#define __TI_TWL6030_H__

void ti_twl6030_init(void);
void ti_twl6030_scan(void);
void ti_twl6030_vmmc_pr(void);
errval_t ti_twl6030_set_vmmc_vsel(int millis);

#endif // __TI_TWL6030_H__
