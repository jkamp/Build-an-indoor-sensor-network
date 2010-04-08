/*
 * Copyright (c) 2007, Swedish Institute of Computer Science.
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
 * $Id: example-collect.c,v 1.10 2010/02/03 21:11:33 adamdunkels Exp $
 */

/**
 * \file
 *         Example of how the collect primitive works.
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#include "contiki.h"
#include "lib/random.h"
#include "net/rime.h"
#include "net/rime/collect.h"
#include "net/rime/neighbor.h"
#include "dev/leds.h"
#include "dev/button-sensor.h"

#include <stdio.h>

static struct collect_conn tc;

/*---------------------------------------------------------------------------*/
PROCESS(example_collect_process, "Test collect process");
AUTOSTART_PROCESSES(&example_collect_process);
/*---------------------------------------------------------------------------*/
static void
recv(const rimeaddr_t *originator, uint8_t seqno, uint8_t hops)
{
  printf("Sink got message from %d.%d, seqno %d, hops %d: len %d '%s'\n",
	 originator->u8[0], originator->u8[1],
	 seqno, hops,
	 packetbuf_datalen(),
	 (char *)packetbuf_dataptr());

}
/*---------------------------------------------------------------------------*/
static const struct collect_callbacks callbacks = { recv };
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_collect_process, ev, data)
{
  PROCESS_BEGIN();

  SENSORS_ACTIVATE(button_sensor);
  
  collect_open(&tc, 130, &callbacks);

  while(1) {
    static struct etimer et;

    /* Send a packet every 16 seconds; first wait 8 seconds, than a
       random time between 0 and 8 seconds. */

    etimer_set(&et, CLOCK_SECOND * 16 + random_rand() % (CLOCK_SECOND * 16));
    PROCESS_WAIT_EVENT();

    if(etimer_expired(&et)) {
      while(tc.forwarding) {
	PROCESS_PAUSE();
      }
      printf("Sending\n");
      packetbuf_clear();
      packetbuf_set_datalen(sprintf(packetbuf_dataptr(),
				  "%s", "Hello") + 1);
      collect_send(&tc, 4);
    }

    if(ev == sensors_event) {
      if(data == &button_sensor) {
	printf("I am sink\n");
	collect_set_sink(&tc, 1);
      }
    }

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
