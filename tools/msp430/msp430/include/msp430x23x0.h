#ifndef __msp430x23x0
#define __msp430x23x0

/* msp430x23x0.h
 *
 * mspgcc project: MSP430 device headers
 * MSP430x22x2 family header
 *
 * (c) 2007 by Steve Underwood <steveu@coppice.org>
 * Originally based in part on work by Texas Instruments Inc.
 *
 * $Id: msp430x23x0.h,v 1.1 2007/07/11 18:04:34 coppice Exp $
 */

#include <iomacros.h>

#define __MSP430_HAS_WDT__
#define __MSP430_HAS_PORT1_R__
#define __MSP430_HAS_PORT2_R__
#define __MSP430_HAS_PORT3_R__
#define __MSP430_HAS_PORT4_R__
#define __MSP430_HAS_TA3__
#define __MSP430_HAS_TB3__
#define __MSP430_HAS_BC2__
#define __MSP430_HAS_FLASH2__
#define __MSP430_HAS_USCI__
#define __MSP430_HAS_CAPLUS__
#define __MSP430_HAS_MPY__

#include <msp430/basic_clock.h>
#include <msp430/flash.h>
#include <msp430/eprom.h>
#include <msp430/timera.h>
#include <msp430/timerb.h>
#include <msp430/gpio.h>
#include <msp430/usci.h>
#include <msp430/compa.h>
#include <msp430/mpy.h>

#include <msp430/common.h>

#define IE1_                0x0000  /* Interrupt Enable 1 */
sfrb(IE1, IE1_);
#define WDTIE               (1<<0)      /* Watchdog Interrupt Enable */
#define OFIE                (1<<1)      /* Osc. Fault  Interrupt Enable */
#define NMIIE               (1<<4)      /* NMI Interrupt Enable */
#define ACCVIE              (1<<5)      /* Flash Access Violation Interrupt Enable */

#define IFG1_               0x0002  /* Interrupt Flag 1 */
sfrb(IFG1, IFG1_);
#define WDTIFG              (1<<0)      /* Watchdog Interrupt Flag */
#define OFIFG               (1<<1)      /* Osc. Fault Interrupt Flag */
#define PORIFG              (1<<2)      /* Power On Interrupt Flag */
#define RSTIFG              (1<<3)      /* Reset Interrupt Flag */
#define NMIIFG              (1<<4)      /* NMI Interrupt Flag */

#define IE2_                0x0001  /* Interrupt Enable 2 */
sfrb(IE2, IE2_);
#define UCA0RXIE            (1<<0)
#define UCA0TXIE            (1<<1)
#define UCB0RXIE            (1<<2)
#define UCB0TXIE            (1<<3)

#define IFG2_               0x0003  /* Interrupt Flag 2 */
sfrb(IFG2,IFG2_);
#define UCA0RXIFG           (1<<0)
#define UCA0TXIFG           (1<<1)
#define UCB0RXIFG           (1<<2)
#define UCB0TXIFG           (1<<3)

/************************************************************
* Calibration Data in Info Mem
************************************************************/

#ifndef __DisableCalData

#define CALDCO_16MHZ_         0x10F8    /* DCOCTL  Calibration Data for 16MHz */
sfrb(CALDCO_16MHZ, CALDCO_16MHZ_);
#define CALBC1_16MHZ_         0x10F9    /* BCSCTL1 Calibration Data for 16MHz */
sfrb(CALBC1_16MHZ, CALBC1_16MHZ_);
#define CALDCO_12MHZ_         0x10FA    /* DCOCTL  Calibration Data for 12MHz */
sfrb(CALDCO_12MHZ, CALDCO_12MHZ_);
#define CALBC1_12MHZ_         0x10FB    /* BCSCTL1 Calibration Data for 12MHz */
sfrb(CALBC1_12MHZ, CALBC1_12MHZ_);
#define CALDCO_8MHZ_          0x10FC    /* DCOCTL  Calibration Data for 8MHz */
sfrb(CALDCO_8MHZ, CALDCO_8MHZ_);
#define CALBC1_8MHZ_          0x10FD    /* BCSCTL1 Calibration Data for 8MHz */
sfrb(CALBC1_8MHZ, CALBC1_8MHZ_);
#define CALDCO_1MHZ_          0x10FE    /* DCOCTL  Calibration Data for 1MHz */
sfrb(CALDCO_1MHZ, CALDCO_1MHZ_);
#define CALBC1_1MHZ_          0x10FF    /* BCSCTL1 Calibration Data for 1MHz */
sfrb(CALBC1_1MHZ, CALBC1_1MHZ_);

#endif /* #ifndef __DisableCalData */

#define PORT1_VECTOR        4       /* 0xFFE4 Port 1 */
#define PORT2_VECTOR        6       /* 0xFFE6 Port 2 */
#define USCIAB0TX_VECTOR    12      /* 0xFFEC USCI A0/B0 Transmit */
#define USCIAB0RX_VECTOR    14      /* 0xFFEE USCI A0/B0 Receive */
#define TIMERA1_VECTOR      16      /* 0xFFF0 Timer A CC1-2, TA */
#define TIMERA0_VECTOR      18      /* 0xFFF2 Timer A CC0 */
#define WDT_VECTOR          20      /* 0xFFF4 Watchdog Timer */
#define COMPARATORA_VECTOR  22      /* 0xFFF6 Comparator A */
#define TIMERB1_VECTOR      24      /* 0xFFF8 Timer B CC1-2, TB */
#define TIMERB0_VECTOR      26      /* 0xFFFA Timer B CC0 */

#define NMI_VECTOR          28      /* 0xFFFC Non-maskable */

#endif /* #ifndef __msp430x22x2 */
