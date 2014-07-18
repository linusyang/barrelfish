/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef DMA_REQUEST_INTERNAL_H
#define DMA_REQUEST_INTERNAL_H

#include <dma/dma_request.h>

/**
 * generic representation of DMA requests
 */
struct dma_request
{
    dma_req_id_t id;            ///<
    dma_req_st_t state;         ///<
    struct dma_req_setup setup; ///<
    struct dma_request *next;   ///<
};

dma_req_id_t dma_request_generate_req_id(struct dma_channel *chan);


/**
 * \brief handles the processing of completed DMA requests
 *
 * \param req   the DMA request to process
 *
 * \returns SYS_ERR_OK on sucess
 *          errval on failure
 */
errval_t dma_request_process(struct dma_request *req);

#endif /* DMA_REQUEST_INTERNAL_H */
