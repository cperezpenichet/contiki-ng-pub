/*
 * Copyright (c) 2018, Uppsala University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         Simple example of carrier-assisted tag broadcast
 * \author
 *         Carlos Perez Penichet
 *         Dilushi Piumwardane
 */

#include "contiki.h"

#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "net/packetbuf.h"
#include "dev/leds.h"
#include "node-id.h"
#include "sys/energest.h"

#include "random.h"

#include "sys/log.h"
#define LOG_MODULE "Tag broadcast"
#define LOG_LEVEL LOG_LEVEL_INFO

#define TDMA 0

#define RTIMER_MILLI (RTIMER_SECOND/1000)
#define SLOT_DURATION (10 * RTIMER_MILLI)
#define MAX_SLOTS 7
#define NUMER_PACKETS 1000

static struct rtimer rt;
static char current_slot;
static char active_slot = 3;
static short seq_no = 0;
static rtimer_clock_t ref_time;
static char in_sync = 0;

PROCESS(transmit_process, "Transmit process");
AUTOSTART_PROCESSES(&transmit_process);

void slot(struct rtimer *rtimer, void* ptr);

static short next_active_slot() {
#if TDMA
	  return (current_slot+1)%MAX_SLOTS;
#else 
	  current_slot = 0;
	  return random_rand() % MAX_SLOTS;
#endif
}
/*---------------------------------------------------------------------------*/
void
input_callback(const void *data, uint16_t len,
		const linkaddr_t *src, const linkaddr_t *dst)
{
    LOG_INFO("Received from ");
    LOG_INFO_LLADDR(src);
    LOG_INFO_(" %s\n",(char *)data);

  ref_time = packetbuf_attr(PACKETBUF_ATTR_TIMESTAMP) + 50*RTIMER_MILLI;

  if (*(char*)data == 'H') {
      if (!in_sync) {
		  rtimer_set(&rt, ref_time, 1, (void (*)(struct rtimer *, void *))slot, NULL);
		  leds_on(LEDS_RED);
		  current_slot = 0;
		  active_slot = next_active_slot();
	  }
  }
} 

void slot(struct rtimer *rtimer, void* ptr) {
    	ref_time += SLOT_DURATION;
	rtimer_set(rtimer, ref_time, 1, (void (*)(struct rtimer *, void *))slot, NULL);
	leds_toggle(LEDS_RED);
	if (!in_sync) {
		in_sync = 1;
	} else if (current_slot == active_slot) {
	  leds_on(LEDS_GREEN);
	  
		if (seq_no < NUMER_PACKETS) {
	          nullnet_buf = (uint8_t*)(&seq_no);
		  nullnet_len = sizeof(seq_no);
		  NETSTACK_NETWORK.output(NULL);
		  seq_no++;
		}
		current_slot = next_active_slot();
	} else {
	  leds_off(LEDS_GREEN);
	  current_slot = (current_slot+1)%MAX_SLOTS;
	}
}

/*---------------------------------------------------------------------------*/
void 
energest_report()
{
    /* Update all energest times. */
    energest_flush();

    printf("\nEnergest:\n");
    printf(" CPU          %lu LPM      %lu DEEP LPM %lud  Total time %lud\n",
	   (unsigned long)energest_type_time(ENERGEST_TYPE_CPU),
	   (unsigned long)energest_type_time(ENERGEST_TYPE_LPM),
	   (unsigned long)energest_type_time(ENERGEST_TYPE_DEEP_LPM),
	   (unsigned long)ENERGEST_GET_TOTAL_TIME());
    printf(" Radio LISTEN %lu TRANSMIT %lu OFF      %lu\n",
	   (unsigned long)energest_type_time(ENERGEST_TYPE_LISTEN),
	   (unsigned long)energest_type_time(ENERGEST_TYPE_TRANSMIT),
	   (unsigned long)(ENERGEST_GET_TOTAL_TIME()
		      - energest_type_time(ENERGEST_TYPE_TRANSMIT)
		      - energest_type_time(ENERGEST_TYPE_LISTEN)));
}
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(transmit_process, ev, data)
{
  static struct  etimer periodic_timer;

  PROCESS_BEGIN();

  nullnet_set_input_callback(input_callback);

  random_init(node_id);

  etimer_set(&periodic_timer, CLOCK_SECOND * 10);
  printf("ENERGEST_SECOND: %u\n", ENERGEST_SECOND);
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    etimer_reset(&periodic_timer);

    if (seq_no >= NUMER_PACKETS) {
      energest_report();
      break;
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/