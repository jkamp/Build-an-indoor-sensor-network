#if !defined(__msp430_headers_usci_h__)
#define __msp430_headers_usci_h__

/* usi.h
 *
 * mspgcc project: MSP430 device headers
 * USCI module header
 *
 * (c) 2006 by Steve Underwood <steveu@coppice.org>
 * Originally based in part on work by Texas Instruments Inc.
 *
 * $Id: usci.h,v 1.3 2006/06/07 13:01:30 coppice Exp $
 */

/* Switches:

__MSP430_HAS_USCI1__ - if device has USCI1

*/

#define USIPE7              (0x80)      /* USI  Port Enable Px.7 */
#define USIPE6              (0x40)      /* USI  Port Enable Px.6 */
#define USIPE5              (0x20)      /* USI  Port Enable Px.5 */
#define USILSB              (0x10)      /* USI  LSB first  1:LSB / 0:MSB */
#define USIMST              (0x08)      /* USI  Master Select  0:Slave / 1:Master */
#define USIGE               (0x04)      /* USI  General Output Enable Latch */
#define USIOE               (0x02)      /* USI  Output Enable */
#define USISWRST            (0x01)      /* USI  Software Reset */

#define USICKPH             (0x80)      /* USI  Sync. Mode: Clock Phase */
#define USII2C              (0x40)      /* USI  I2C Mode */ 
#define USISTTIE            (0x20)      /* USI  START Condition interrupt enable */
#define USIIE               (0x10)      /* USI  Counter Interrupt enable */
#define USIAL               (0x08)      /* USI  Arbitration Lost */
#define USISTP              (0x04)      /* USI  STOP Condition received */
#define USISTTIFG           (0x02)      /* USI  START Condition interrupt Flag */
#define USIIFG              (0x01)      /* USI  Counter Interrupt Flag */

#define USIDIV2             (0x80)      /* USI  Clock Divider 2 */
#define USIDIV1             (0x40)      /* USI  Clock Divider 1 */ 
#define USIDIV0             (0x20)      /* USI  Clock Divider 0 */
#define USISSEL2            (0x10)      /* USI  Clock Source Select 2 */
#define USISSEL1            (0x08)      /* USI  Clock Source Select 1 */
#define USISSEL0            (0x04)      /* USI  Clock Source Select 0 */
#define USICKPL             (0x02)      /* USI  Clock Polarity 0:Inactive=Low / 1:Inactive=High */
#define USISWCLK            (0x01)      /* USI  Software Clock */

#define USIDIV_0            (0x00)      /* USI  Clock Divider: 0 */
#define USIDIV_1            (0x20)      /* USI  Clock Divider: 1 */
#define USIDIV_2            (0x40)      /* USI  Clock Divider: 2 */
#define USIDIV_3            (0x60)      /* USI  Clock Divider: 3 */
#define USIDIV_4            (0x80)      /* USI  Clock Divider: 4 */
#define USIDIV_5            (0xA0)      /* USI  Clock Divider: 5 */
#define USIDIV_6            (0xC0)      /* USI  Clock Divider: 6 */
#define USIDIV_7            (0xE0)      /* USI  Clock Divider: 7 */

#define USISSEL_0           (0x00)      /* USI  Clock Source: 0 */
#define USISSEL_1           (0x04)      /* USI  Clock Source: 1 */
#define USISSEL_2           (0x08)      /* USI  Clock Source: 2 */
#define USISSEL_3           (0x0C)      /* USI  Clock Source: 3 */
#define USISSEL_4           (0x10)      /* USI  Clock Source: 4 */
#define USISSEL_5           (0x14)      /* USI  Clock Source: 5 */
#define USISSEL_6           (0x18)      /* USI  Clock Source: 6 */
#define USISSEL_7           (0x1C)      /* USI  Clock Source: 7 */

#define USISCLREL           (0x80)      /* USI  SCL Released */
#define USI16B              (0x40)      /* USI  16 Bit Shift Register Enable */ 
#define USIFGDC             (0x20)      /* USI  Interrupt Flag don't clear */
#define USICNT4             (0x10)      /* USI  Bit Count 4 */
#define USICNT3             (0x08)      /* USI  Bit Count 3 */
#define USICNT2             (0x04)      /* USI  Bit Count 2 */
#define USICNT1             (0x02)      /* USI  Bit Count 1 */
#define USICNT0             (0x01)      /* USI  Bit Count 0 */

// UART-Mode Bits
#define UCPEN               (0x80)      /* Async. Mode: Parity enable */
#define UCPAR               (0x40)      /* Async. Mode: Parity     0:odd / 1:even */
#define UCMSB               (0x20)      /* Async. Mode: MSB first  0:LSB / 1:MSB */
#define UC7BIT              (0x10)      /* Async. Mode: Data Bits  0:8-bits / 1:7-bits */
#define UCSPB               (0x08)      /* Async. Mode: Stop Bits  0:one / 1: two */
#define UCMODE1             (0x04)      /* Async. Mode: USCI Mode 1 */
#define UCMODE0             (0x02)      /* Async. Mode: USCI Mode 0 */
#define UCSYNC              (0x01)      /* Sync-Mode  0:UART-Mode / 1:SPI-Mode */

// SPI-Mode Bits
#define UCCKPH              (0x80)      /* Sync. Mode: Clock Phase */
#define UCCKPL              (0x40)      /* Sync. Mode: Clock Polarity */
#define UCMST               (0x08)      /* Sync. Mode: Master Select */

// I2C-Mode Bits
#define UCA10               (0x80)      /* 10-bit Address Mode */
#define UCSLA10             (0x40)      /* 10-bit Slave Address Mode */
#define UCMM                (0x20)      /* Multi-Master Environment */
//#define res               (0x10)      /* reserved */
#define UCMODE_0            (0<<1)      /* Sync. Mode: USCI Mode: 0 */
#define UCMODE_1            (1<<1)      /* Sync. Mode: USCI Mode: 1 */
#define UCMODE_2            (2<<1)      /* Sync. Mode: USCI Mode: 2 */
#define UCMODE_3            (3<<1)      /* Sync. Mode: USCI Mode: 3 */

// UART-Mode Bits
#define UCSSEL1             (0x80)      /* USCI 0 Clock Source Select 1 */
#define UCSSEL0             (0x40)      /* USCI 0 Clock Source Select 0 */
#define UCRXEIE             (0x20)      /* RX Error interrupt enable */
#define UCBRKIE             (0x10)      /* Break interrupt enable */
#define UCDORM              (0x08)      /* Dormant (Sleep) Mode */
#define UCTXADDR            (0x04)      /* Send next Data as Address */
#define UCTXBRK             (0x02)      /* Send next Data as Break */
#define UCSWRST             (0x01)      /* USCI Software Reset */

// SPI-Mode Bits
//#define res               (0x20)      /* reserved */
//#define res               (0x10)      /* reserved */
//#define res               (0x08)      /* reserved */
//#define res               (0x04)      /* reserved */
//#define res               (0x02)      /* reserved */

// I2C-Mode Bits
//#define res               (0x20)      /* reserved */
#define UCTR                (0x10)      /* Transmit/Receive Select/Flag */
#define UCTXNACK            (0x08)      /* Transmit NACK */
#define UCTXSTP             (0x04)      /* Transmit STOP */
#define UCTXSTT             (0x02)      /* Transmit START */
#define UCSSEL_0            (0<<6)      /* USCI 0 Clock Source: 0 */
#define UCSSEL_1            (1<<6)      /* USCI 0 Clock Source: 1 */
#define UCSSEL_2            (2<<6)      /* USCI 0 Clock Source: 2 */
#define UCSSEL_3            (3<<6)      /* USCI 0 Clock Source: 3 */

#define UCBRF3              (0x80)      /* USCI First Stage Modulation Select 3 */
#define UCBRF2              (0x40)      /* USCI First Stage Modulation Select 2 */
#define UCBRF1              (0x20)      /* USCI First Stage Modulation Select 1 */
#define UCBRF0              (0x10)      /* USCI First Stage Modulation Select 0 */
#define UCBRS2              (0x08)      /* USCI Second Stage Modulation Select 2 */
#define UCBRS1              (0x04)      /* USCI Second Stage Modulation Select 1 */
#define UCBRS0              (0x02)      /* USCI Second Stage Modulation Select 0 */
#define UCOS16              (0x01)      /* USCI 16-times Oversampling enable */

#define UCBRF_0             (0x0<<4)    /* USCI First Stage Modulation: 0 */
#define UCBRF_1             (0x1<<4)    /* USCI First Stage Modulation: 1 */
#define UCBRF_2             (0x2<<4)    /* USCI First Stage Modulation: 2 */
#define UCBRF_3             (0x3<<4)    /* USCI First Stage Modulation: 3 */
#define UCBRF_4             (0x4<<4)    /* USCI First Stage Modulation: 4 */
#define UCBRF_5             (0x5<<4)    /* USCI First Stage Modulation: 5 */
#define UCBRF_6             (0x6<<4)    /* USCI First Stage Modulation: 6 */
#define UCBRF_7             (0x7<<4)    /* USCI First Stage Modulation: 7 */
#define UCBRF_8             (0x8<<4)    /* USCI First Stage Modulation: 8 */
#define UCBRF_9             (0x9<<4)    /* USCI First Stage Modulation: 9 */
#define UCBRF_10            (0xA<<4)    /* USCI First Stage Modulation: A */
#define UCBRF_11            (0xB<<4)    /* USCI First Stage Modulation: B */
#define UCBRF_12            (0xC<<4)    /* USCI First Stage Modulation: C */
#define UCBRF_13            (0xD<<4)    /* USCI First Stage Modulation: D */
#define UCBRF_14            (0xE<<4)    /* USCI First Stage Modulation: E */
#define UCBRF_15            (0xF<<4)    /* USCI First Stage Modulation: F */

#define UCBRS_0             (0<<1)      /* USCI Second Stage Modulation: 0 */
#define UCBRS_1             (1<<1)      /* USCI Second Stage Modulation: 1 */
#define UCBRS_2             (2<<1)      /* USCI Second Stage Modulation: 2 */
#define UCBRS_3             (3<<1)      /* USCI Second Stage Modulation: 3 */
#define UCBRS_4             (4<<1)      /* USCI Second Stage Modulation: 4 */
#define UCBRS_5             (5<<1)      /* USCI Second Stage Modulation: 5 */
#define UCBRS_6             (6<<1)      /* USCI Second Stage Modulation: 6 */
#define UCBRS_7             (7<<1)      /* USCI Second Stage Modulation: 7 */

#define UCLISTEN            (0x80)      /* USCI Listen mode */
#define UCFE                (0x40)      /* USCI Frame Error Flag */
#define UCOE                (0x20)      /* USCI Overrun Error Flag */
#define UCPE                (0x10)      /* USCI Parity Error Flag */
#define UCBRK               (0x08)      /* USCI Break received */
#define UCRXERR             (0x04)      /* USCI RX Error Flag */
#define UCADDR              (0x02)      /* USCI Address received Flag */
#define UCBUSY              (0x01)      /* USCI Busy Flag */
#define UCIDLE              (0x02)      /* USCI Idle line detected Flag */

//#define res               (0x80)      /* reserved */
//#define res               (0x40)      /* reserved */
//#define res               (0x20)      /* reserved */
//#define res               (0x10)      /* reserved */
#define UCNACKIE            (0x08)      /* NACK Condition interrupt enable */
#define UCSTPIE             (0x04)      /* STOP Condition interrupt enable */
#define UCSTTIE             (0x02)      /* START Condition interrupt enable */
#define UCALIE              (0x01)      /* Arbitration Lost interrupt enable */

#define UCSCLLOW            (0x40)      /* SCL low */
#define UCGC                (0x20)      /* General Call address received Flag */
#define UCBBUSY             (0x10)      /* Bus Busy Flag */
#define UCNACKIFG           (0x08)      /* NAK Condition interrupt Flag */
#define UCSTPIFG            (0x04)      /* STOP Condition interrupt Flag */
#define UCSTTIFG            (0x02)      /* START Condition interrupt Flag */
#define UCALIFG             (0x01)      /* Arbitration Lost interrupt Flag */

#define UCIRTXPL5           (0x80)      /* IRDA Transmit Pulse Length 5 */
#define UCIRTXPL4           (0x40)      /* IRDA Transmit Pulse Length 4 */
#define UCIRTXPL3           (0x20)      /* IRDA Transmit Pulse Length 3 */
#define UCIRTXPL2           (0x10)      /* IRDA Transmit Pulse Length 2 */
#define UCIRTXPL1           (0x08)      /* IRDA Transmit Pulse Length 1 */
#define UCIRTXPL0           (0x04)      /* IRDA Transmit Pulse Length 0 */
#define UCIRTXCLK           (0x02)      /* IRDA Transmit Pulse Clock Select */
#define UCIREN              (0x01)      /* IRDA Encoder/Decoder enable */

#define UCIRRXFL5           (0x80)      /* IRDA Receive Filter Length 5 */
#define UCIRRXFL4           (0x40)      /* IRDA Receive Filter Length 4 */
#define UCIRRXFL3           (0x20)      /* IRDA Receive Filter Length 3 */
#define UCIRRXFL2           (0x10)      /* IRDA Receive Filter Length 2 */
#define UCIRRXFL1           (0x08)      /* IRDA Receive Filter Length 1 */
#define UCIRRXFL0           (0x04)      /* IRDA Receive Filter Length 0 */
#define UCIRRXPL            (0x02)      /* IRDA Receive Input Polarity */
#define UCIRRXFE            (0x01)      /* IRDA Receive Filter enable */

//#define res                 (0x80)    /* reserved */
//#define res                 (0x40)    /* reserved */
#define UCDELIM1            (0x20)      /* Break Sync Delimiter 1 */
#define UCDELIM0            (0x10)      /* Break Sync Delimiter 0 */
#define UCSTOE              (0x08)      /* Sync-Field Timeout error */
#define UCBTOE              (0x04)      /* Break Timeout error */
//#define res                 (0x02)    /* reserved */
#define UCABDEN             (0x01)      /* Auto Baud Rate detect enable */

#define UCGCEN              (0x8000)    /* I2C General Call enable */
#define UCOA9               (0x0200)    /* I2C Own Address 9 */
#define UCOA8               (0x0100)    /* I2C Own Address 8 */
#define UCOA7               (0x0080)    /* I2C Own Address 7 */
#define UCOA6               (0x0040)    /* I2C Own Address 6 */
#define UCOA5               (0x0020)    /* I2C Own Address 5 */
#define UCOA4               (0x0010)    /* I2C Own Address 4 */
#define UCOA3               (0x0008)    /* I2C Own Address 3 */
#define UCOA2               (0x0004)    /* I2C Own Address 2 */
#define UCOA1               (0x0002)    /* I2C Own Address 1 */
#define UCOA0               (0x0001)    /* I2C Own Address 0 */

#define UCSA9               (0x0200)    /* I2C Slave Address 9 */
#define UCSA8               (0x0100)    /* I2C Slave Address 8 */
#define UCSA7               (0x0080)    /* I2C Slave Address 7 */
#define UCSA6               (0x0040)    /* I2C Slave Address 6 */
#define UCSA5               (0x0020)    /* I2C Slave Address 5 */
#define UCSA4               (0x0010)    /* I2C Slave Address 4 */
#define UCSA3               (0x0008)    /* I2C Slave Address 3 */
#define UCSA2               (0x0004)    /* I2C Slave Address 2 */
#define UCSA1               (0x0002)    /* I2C Slave Address 1 */
#define UCSA0               (0x0001)    /* I2C Slave Address 0 */

/* -------- USCI0 */

#define UCA0CTL0_           0x0060      /* USCI A0 Control Register 0 */
sfrb(UCA0CTL0, UCA0CTL0_);
#define UCA0CTL1_           0x0061      /* USCI A0 Control Register 1 */
sfrb(UCA0CTL1, UCA0CTL1_);
#define UCA0BR0_            0x0062      /* USCI A0 Baud Rate 0 */
sfrb(UCA0BR0, UCA0BR0_);
#define UCA0BR1_            0x0063      /* USCI A0 Baud Rate 1 */
sfrb(UCA0BR1, UCA0BR1_);
#define UCA0MCTL_           0x0064      /* USCI A0 Modulation Control */
sfrb(UCA0MCTL, UCA0MCTL_);
#define UCA0STAT_           0x0065      /* USCI A0 Status Register */
sfrb(UCA0STAT, UCA0STAT_);
#define UCA0RXBUF_          0x0066      /* USCI A0 Receive Buffer */
/*READ_ONLY*/ sfrb(UCA0RXBUF, UCA0RXBUF_);
#define UCA0TXBUF_          0x0067      /* USCI A0 Transmit Buffer */
sfrb(UCA0TXBUF, UCA0TXBUF_);
#define UCA0ABCTL_          0x005D      /* USCI A0 Auto baud/LIN Control */
sfrb(UCA0ABCTL, UCA0ABCTL_);
#define UCA0IRTCTL_         0x005E      /* USCI A0 IrDA Transmit Control */
sfrb(UCA0IRTCTL, UCA0IRTCTL_);
#define UCA0IRRCTL_         0x005F      /* USCI A0 IrDA Receive Control */
sfrb(UCA0IRRCTL, UCA0IRRCTL_);

#define UCB0CTL0_           0x0068      /* USCI B0 Control Register 0 */
sfrb(UCB0CTL0, UCB0CTL0_);
#define UCB0CTL1_           0x0069      /* USCI B0 Control Register 1 */
sfrb(UCB0CTL1, UCB0CTL1_);
#define UCB0BR0_            0x006A      /* USCI B0 Baud Rate 0 */
sfrb(UCB0BR0, UCB0BR0_);
#define UCB0BR1_            0x006B      /* USCI B0 Baud Rate 1 */
sfrb(UCB0BR1, UCB0BR1_);
#define UCB0I2CIE_          0x006C      /* USCI B0 I2C Interrupt Enable Register */
sfrb(UCB0I2CIE, UCB0I2CIE_);
#define UCB0STAT_           0x006D      /* USCI B0 Status Register */
sfrb(UCB0STAT, UCB0STAT_);
#define UCB0RXBUF_          0x006E      /* USCI B0 Receive Buffer */
/*READ_ONLY*/ sfrb(UCB0RXBUF, UCB0RXBUF_);
#define UCB0TXBUF_          0x006F      /* USCI B0 Transmit Buffer */
sfrb(UCB0TXBUF, UCB0TXBUF_);
#define UCB0I2COA_          0x0118      /* USCI B0 I2C Own Address */
sfrw(UCB0I2COA, UCB0I2COA_);
#define UCB0I2CSA_          0x011A      /* USCI B0 I2C Slave Address */
sfrw(UCB0I2CSA, UCB0I2CSA_);

#if defined(__MSP430_HAS_USCI1__)

/* -------- USCI1 */

#define UCA1CTL0_           0x00D0      /* USCI A1 Control Register 0 */
sfrb(UCA1CTL0, UCA1CTL0_);
#define UCA1CTL1_           0x00D1      /* USCI A1 Control Register 1 */
sfrb(UCA1CTL1, UCA1CTL1_);
#define UCA1BR0_            0x00D2      /* USCI A1 Baud Rate 0 */
sfrb(UCA1BR0, UCA1BR0_);
#define UCA1BR1_            0x00D3      /* USCI A1 Baud Rate 1 */
sfrb(UCA1BR1, UCA1BR1_);
#define UCA1MCTL_           0x00D4      /* USCI A1 Modulation Control */
sfrb(UCA1MCTL, UCA1MCTL_);
#define UCA1STAT_           0x00D5      /* USCI A1 Status Register */
sfrb(UCA1STAT, UCA1STAT_);
#define UCA1RXBUF_          0x00D6      /* USCI A1 Receive Buffer */
/*READ_ONLY*/ sfrb(UCA1RXBUF, UCA1RXBUF_);
#define UCA1TXBUF_          0x00D7      /* USCI A1 Transmit Buffer */
sfrb(UCA1TXBUF, UCA1TXBUF_);
#define UCA1ABCTL_          0x00CD      /* USCI A1 Auto baud/LIN Control */
sfrb(UCA1ABCTL, UCA1ABCTL_);
#define UCA1IRTCTL_         0x00CE      /* USCI A1 IrDA Transmit Control */
sfrb(UCA1IRTCTL, UCA1IRTCTL_);
#define UCA1IRRCTL_         0x00CF      /* USCI A1 IrDA Receive Control */
sfrb(UCA1IRRCTL, UCA1IRRCTL_);

#define UCB1CTL0_           0x00D8      /* USCI B1 Control Register 0 */
sfrb(UCB1CTL0, UCB1CTL0_);
#define UCB1CTL1_           0x00D9      /* USCI B1 Control Register 1 */
sfrb(UCB1CTL1, UCB1CTL1_);
#define UCB1BR0_            0x00DA      /* USCI B1 Baud Rate 0 */
sfrb(UCB1BR0, UCB1BR0_);
#define UCB1BR1_            0x00DB      /* USCI B1 Baud Rate 1 */
sfrb(UCB1BR1, UCB1BR1_);
#define UCB1I2CIE_          0x00DC      /* USCI B1 I2C Interrupt Enable Register */
sfrb(UCB1I2CIE, UCB1I2CIE_);
#define UCB1STAT_           0x00DD      /* USCI B1 Status Register */
sfrb(UCB1STAT, UCB1STAT_);
#define UCB1RXBUF_          0x00DE      /* USCI B1 Receive Buffer */
/*READ_ONLY*/ sfrb(UCB1RXBUF, UCB1RXBUF_);
#define UCB1TXBUF_          0x00DF      /* USCI B1 Transmit Buffer */
sfrb(UCB1TXBUF, UCB1TXBUF_);
#define UCB1I2COA_          0x017C      /* USCI B1 I2C Own Address */
sfrw(UCB1I2COA, UCB1I2COA_);
#define UCB1I2CSA_          0x017E      /* USCI B1 I2C Slave Address */
sfrw(UCB1I2CSA, UCB1I2CSA_);

#define UC1IE_              0x0006      /* USCI A1/B1 Interrupt enable register */
sfrb(UC1IE, UC1IE_);
#define UC1IFG_             0x0007      /* USCI A1/B1 Interrupt flag register */
sfrb(UC1IFG, UC1IFG_);

#endif /* __MSP430_HAS_USCI1__ */

#endif
