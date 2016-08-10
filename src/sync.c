/*
 *    Copyright (c) 
 *      (c) 2016      Chintalagiri Shashank, Firefly Aerospace Pvt.Ltd.
 *      (c) 2010-2011 Chintalagiri Shashank, Jugnu, IIT Kanpur
 * 
 *    This file is part of
 *    Embedded bootstraps : time library
 * 
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation; either version 3 of the License, or
 *    (at your option) any later version.
 * 
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Lesser General Public License for more details.
 * 
 *    You should have received a copy of the GNU Lesser General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


/**
 * @file sync.c
 * @brief Time synchonization function implementations.
 * 
 * @see sync.h
 */


#include <ucdm/ucdm.h>
#include "time.h"
#include "sync.h"

avlt_node_t  tm_avlt_sync_handler_node;
tm_sync_sm_t tm_sync_sm;

void tm_sync_init(uint16_t handlers_base_address){
    // Setup Host Interface Registers
    for(uint8_t i=0; i<4; i++){
        ucdm_enable_regw(handlers_base_address + i);        
    }
    // Setup Host Sync Handler(s)
    ucdm_install_regw_handler(handlers_base_address + 3, 
                              &tm_avlt_sync_handler_node, 
                              &tm_sync_handler);
    tm_sync_sm.state = TM_SYNC_STATE_IDLE;
}

void tm_sync_request_host(void){
    ucdm_exception_status |= UCDM_EXST_TIMESYNC_REQ;
    tm_sync_sm.state = TM_SYNC_STATE_WAIT_HOST;
}

static inline void tm_sync_apply(void);
static inline void tm_sync_apply(void){
    tm_sdelta_t offset, tsd1, tsd2;
    uint8_t cfrac = 0;
    tm_get_sdelta(&(tm_sync_sm.t1), &(tm_sync_sm.t1p), &(tsd1));
    tm_get_sdelta(&(tm_sync_sm.t2p), &(tm_sync_sm.t2), &(tsd2));
    if (tsd1.sgn == tsd2.sgn){
        offset.seconds = (tsd1.seconds + tsd2.seconds) / 2;
        offset.frac = (tsd1.frac + tsd2.frac) / 2;
        while (offset.frac >= TIME_TICKS_PER_SECOND){
            offset.frac -= TIME_TICKS_PER_SECOND;
            offset.seconds += 1;
        }
        offset.sgn = tsd1.sgn;
    }
    else{
        if (tsd2.frac > tsd1.frac){
            cfrac = 0;
            offset.frac = tsd2.frac - tsd1.frac;
        }
        else{
            cfrac = 1;
            offset.frac = tsd1.frac - tsd2.frac;
        }
        
        if (tsd2.seconds == tsd1.seconds){
            offset.seconds = 0; 
            offset.sgn = cfrac;
        }
        else if (tsd2.seconds > tsd1.seconds){
            offset.sgn = 0;
            offset.seconds = tsd2.seconds - offset.seconds - cfrac;
        }
        else{
            offset.sgn = 1;
            offset.seconds = tsd1.seconds - tsd2.seconds + cfrac - 1;
        }
    }
    if (offset.sgn){
        offset.sgn = 0;
    }
    else{
        offset.sgn = 1;
    }
    tm_apply_sdelta(&tm_current, &offset);
}


void tm_sync_handler(uint16_t addr){
    switch(tm_sync_sm.state){
        case TM_SYNC_STATE_PREINIT:
            break;
        case TM_SYNC_STATE_WAIT_HOST:
            ucdm_exception_status &= ~UCDM_EXST_TIMESYNC_REQ;
        case TM_SYNC_STATE_IDLE:
            tm_sync_sm.state = TM_SYNC_STATE_WAIT_DELAY_OUT;
            // Got the sync timestamp from the host. 
            tm_sync_sm.t1.seconds = ((uint32_t)(ucdm_register[addr-3].data) << 16) |
                                     (uint32_t)(ucdm_register[addr-2].data);
            tm_sync_sm.t1.frac = ucdm_register[addr].data;
            tm_current_time(&(tm_sync_sm.t1p));
            break;
        case TM_SYNC_STATE_WAIT_DELAY_OUT:
            tm_sync_sm.state = TM_SYNC_STATE_WAIT_DELAY_IN;
            // Host read our timestamp for delay calculation.
            // Store the timestamp sent out
            tm_current_time(&(tm_sync_sm.t2));
            break;
        case TM_SYNC_STATE_WAIT_DELAY_IN:
            // Host returned its timestamp for delay calculation.
            tm_sync_sm.t2p.seconds = ((uint32_t)(ucdm_register[addr-3].data) << 16) | 
                                      (uint32_t)(ucdm_register[addr-2].data);
            tm_sync_sm.t2p.frac = ucdm_register[addr].data;
            tm_sync_sm.state = TM_SYNC_STATE_IDLE;
            // All information is now available. Calculate and apply.
            tm_sync_apply();
            break;
        default:
            break;
    }
    return;
}
