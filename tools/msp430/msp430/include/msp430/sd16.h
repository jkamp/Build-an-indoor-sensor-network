#if !defined(__msp430_headers_sd16_h__)
#define __msp430_headers_sd16_h__

/* sd16.h
 *
 * mspgcc project: MSP430 device headers
 * Sigma Delta 16 module header
 *
 * (c) 2003 by Steve Underwood <steveu@coppice.org>
 * Originally based in part on work by Texas Instruments Inc.
 *
 * $Id: sd16.h,v 1.8 2006/12/26 10:33:14 coppice Exp $
 */

/* Switches:

__msp430_have_sd16a - the SD16 is the "A" type
__msp430_have_sd16_1
__msp430_have_sd16_2
*/

#define SD16CTL_            0x0100      /* Sigma Delta ADC 16 Control Register */
sfrw(SD16CTL, SD16CTL_);
#define SD16IV_             0x0110      /* SD16 Interrupt Vector Register */
sfrw(SD16IV, SD16IV_);

#if defined(__msp430_have_sd16a)
#define SD16AE_             0x00B7      /* SD16 Analog Input Enable Register */
sfrb(SD16AE, SD16AE_);
#else
#define SD16CONF0_          0x00B7      /* SD16 Internal Configuration Register 0 */
sfrb(SD16CONF0, SD16CONF0_);
#define SD16CONF1_          0x00BF      /* SD16 Internal Configuration Register 1 */
sfrb(SD16CONF1, SD16CONF1_);
                                        /* Please use only the recommended settings */
#endif

#define SD16INCTL0_         0x00B0      /* SD16 Input Control Register Channel 0 */
sfrb(SD16INCTL0, SD16INCTL0_);
#define SD16PRE0_           0x00B8      /* SD16 Preload Register Channel 0 */
sfrb(SD16PRE0, SD16PRE0_);
#define SD16CCTL0_          0x0102      /* SD16 Channel 0 Control Register */
sfrw(SD16CCTL0, SD16CCTL0_);
#define SD16MEM0_           0x0112      /* SD16 Channel 0 Conversion Memory */
sfrw(SD16MEM0, SD16MEM0_);

#if defined(__msp430_have_sd16_1)
#define SD16INCTL1_         0x00B1      /* SD16 Input Control Register Channel 1 */
sfrb(SD16INCTL1, SD16INCTL1_);
#define SD16PRE1_           0x00B9      /* SD16 Preload Register Channel 1 */
sfrb(SD16PRE1, SD16PRE1_);
#define SD16CCTL1_          0x0104      /* SD16 Channel 1 Control Register */
sfrw(SD16CCTL1, SD16CCTL1_);
#define SD16MEM1_           0x0114      /* SD16 Channel 1 Conversion Memory */
sfrw(SD16MEM1, SD16MEM1_);
#endif

#if defined(__msp430_have_sd16_2)
#define SD16INCTL2_         0x00B2      /* SD16 Input Control Register Channel 2 */
sfrb(SD16INCTL2, SD16INCTL2_);
#define SD16PRE2_           0x00BA      /* SD16 Preload Register Channel 2 */
sfrb(SD16PRE2, SD16PRE2_);
#define SD16CCTL2_          0x0106      /* SD16 Channel 2 Control Register */
sfrw(SD16CCTL2, SD16CCTL2_);
#define SD16MEM2_           0x0116      /* SD16 Channel 2 Conversion Memory */
sfrw(SD16MEM2, SD16MEM2_);
#endif

#if defined(__msp430_have_sd16a)
/* SD16AE */
#define SD16AE0             0x0001      /* SD16 External Input Enable 0 */
#define SD16AE1             0x0002      /* SD16 External Input Enable 1 */
#define SD16AE2             0x0004      /* SD16 External Input Enable 2 */
#define SD16AE3             0x0008      /* SD16 External Input Enable 3 */
#define SD16AE4             0x0010      /* SD16 External Input Enable 4 */
#define SD16AE5             0x0020      /* SD16 External Input Enable 5 */
#define SD16AE6             0x0040      /* SD16 External Input Enable 6 */
#define SD16AE7             0x0080      /* SD16 External Input Enable 7 */
#endif

/* SD16INCTLx - AFEINCTLx */
#define SD16INCH0           0x0001      /* SD16 Input Channel select 0 */
#define SD16INCH1           0x0002      /* SD16 Input Channel select 1 */
#define SD16INCH2           0x0004      /* SD16 Input Channel select 2 */
#define SD16GAIN0           0x0008      /* AFE Input Pre-Amplifier Gain Select 0 */
#define SD16GAIN1           0x0010      /* AFE Input Pre-Amplifier Gain Select 1 */
#define SD16GAIN2           0x0020      /* AFE Input Pre-Amplifier Gain Select 2 */
#define SD16INTDLY0         0x0040      /* SD16 Interrupt Delay after 1.Conversion 0 */
#define SD16INTDLY1         0x0080      /* SD16 Interrupt Delay after 1.Conversion 1 */

#define SD16GAIN_1          (0<<3)      /* AFE Input Pre-Amplifier Gain Select *1  */
#define SD16GAIN_2          (1<<3)      /* AFE Input Pre-Amplifier Gain Select *2  */
#define SD16GAIN_4          (2<<3)      /* AFE Input Pre-Amplifier Gain Select *4  */
#define SD16GAIN_8          (3<<3)      /* AFE Input Pre-Amplifier Gain Select *8  */
#define SD16GAIN_16         (4<<3)      /* AFE Input Pre-Amplifier Gain Select *16 */
#define SD16GAIN_32         (5<<3)      /* AFE Input Pre-Amplifier Gain Select *32 */

#define SD16INCH_0          (0<<0)      /* SD16 Input Channel select input */
#define SD16INCH_1          (1<<0)      /* SD16 Input Channel select input */
#define SD16INCH_2          (2<<0)      /* SD16 Input Channel select input */
#define SD16INCH_3          (3<<0)      /* SD16 Input Channel select input */
#define SD16INCH_4          (4<<0)      /* SD16 Input Channel select input */
#define SD16INCH_5          (5<<0)      /* SD16 Input Channel select input */
#define SD16INCH_6          (6<<0)      /* SD16 Input Channel select Temp */
#define SD16INCH_7          (7<<0)      /* SD16 Input Channel select Offset */

#define SD16INTDLY_0        (0<<6)      /* SD16 Interrupt Delay: Int. after 4th conversion  */
#define SD16INTDLY_1        (1<<6)      /* SD16 Interrupt Delay: Int. after 3rd conversion  */
#define SD16INTDLY_2        (2<<6)      /* SD16 Interrupt Delay: Int. after 2nd conversion  */
#define SD16INTDLY_3        (3<<6)      /* SD16 Interrupt Delay: Int. after 1st conversion  */

/* SD16CTL - AFECTL */
#define SD16OVIE            0x0002      /* Overflow Interupt Enable */
#define SD16REFON           0x0004      /* Switch internal Reference on */
#define SD16VMIDON          0x0008      /* Switch Vmid Buffer on */
#define SD16SSEL0           0x0010      /* SD16 Clock Source Select 0 */
#define SD16SSEL1           0x0020      /* SD16 Clock Source Select 1 */
#define SD16DIV0            0x0040      /* SD16 Clock Divider Select 0 */
#define SD16DIV1            0x0080      /* SD16 Clock Divider Select 1 */
#define SD16LP              0x0100      /* SD16 Low Power Mode Enable */
#if defined(__msp430_have_sd16a)
#define SD16XDIV0           0x0200      /* SD16 2.Clock Divider Select 0 */
#define SD16XDIV1           0x0400      /* SD16 2.Clock Divider Select 1 */
//#define SD16XDIV2           0x0800)     /* SD16 2.Clock Divider Select 2 */
#endif

#define SD16DIV_0           (0x0000)               /* SD16 Clock Divider Select /1 */
#define SD16DIV_1           (SD16DIV0)             /* SD16 Clock Divider Select /2 */
#define SD16DIV_2           (SD16DIV1)             /* SD16 Clock Divider Select /4 */
#define SD16DIV_3           (SD16DIV0|SD16DIV1)    /* SD16 Clock Divider Select /8 */

#if defined(__msp430_have_sd16a)
#define SD16XDIV_0          (0x0000)               /* SD16 2.Clock Divider Select /1 */
#define SD16XDIV_1          (SD16XDIV0)            /* SD16 2.Clock Divider Select /3 */
#define SD16XDIV_2          (SD16XDIV1)            /* SD16 2.Clock Divider Select /16 */
#define SD16XDIV_3          (SD16XDIV0|SD16XDIV1)  /* SD16 2.Clock Divider Select /48 */
#endif

#define SD16SSEL_0          (0x0000)               /* AFE Clock Source Select MCLK  */
#define SD16SSEL_1          (SD16SSEL0)            /* AFE Clock Source Select SMCLK */
#define SD16SSEL_2          (SD16SSEL1)            /* AFE Clock Source Select ACLK  */
#define SD16SSEL_3          (SD16SSEL0|SD16SSEL1)  /* AFE Clock Source Select TACLK */

/* SD16CCTLx - AFECCTLx */
#define SD16GRP             0x0001      /* Grouping of Channels: 0:Off/1:On */
#define SD16SC              0x0002      /* Start Conversion */
#define SD16IFG             0x0004      /* SD16 Channel x Interrupt Flag */
#define SD16IE              0x0008      /* SD16 Channel x Interrupt Enable */
#define SD16DF              0x0010      /* SD16 Channel x Data Format: 0:Unipolar/1:Bipolar */
#define SD16OVIFG           0x0020      /* SD16 Channel x Overflow Interrupt Flag */
#define SD16LSBACC          0x0040      /* SD16 Channel x Access LSB of ADC */
#define SD16LSBTOG          0x0080      /* SD16 Channel x Toggle LSB Output of ADC */
#define SD16OSR0            0x0100      /* SD16 Channel x OverSampling Ratio 0 */
#define SD16OSR1            0x0200      /* SD16 Channel x OverSampling Ratio 1 */
#define SD16SNGL            0x0400      /* SD16 Channel x Single Conversion On/Off */
#if defined(__msp430_have_sd16a)
#define SD16XOSR            0x0800      /* SD16 Channel x Extended OverSampling Ratio */
#define SD16UNI             0x1000      /* SD16 Channel x Bipolar(0) / Unipolar(1) Mode */
#define SD16BUF0            0x2000      /* SD16 Channel x High Impedance Input Buffer Select: 0 */
#define SD16BUF1            0x4000      /* SD16 Channel x High Impedance Input Buffer Select: 1 */
#define SD16BUFG            0x8000      /* SD16 Channel x Buffer Gain 0:Gain=1 / 1:Gain=2 */
#endif

#if defined(__msp430_have_sd16a)
#define SD16OSR_1024        (SD16XOSR|SD16OSR0) /* SD16 Channel x OverSampling Ratio 1024 */
#define SD16OSR_512         (SD16XOSR)          /* SD16 Channel x OverSampling Ratio 512 */
#endif
#define SD16OSR_256         (0<<8)              /* SD16 Channel x OverSampling Ratio 256 */
#define SD16OSR_128         (1<<8)              /* SD16 Channel x OverSampling Ratio 128 */
#define SD16OSR_64          (2<<8)              /* SD16 Channel x OverSampling Ratio  64 */
#define SD16OSR_32          (3<<8)              /* SD16 Channel x OverSampling Ratio  32 */

#if defined(__msp430_have_sd16a)
#define SD16BUF_0           (0<<13)             /* SD16 High Imp. Input Buffer: Disabled */
#define SD16BUF_1           (1<<13)             /* SD16 High Imp. Input Buffer: Slow */
#define SD16BUF_2           (2<<13)             /* SD16 High Imp. Input Buffer: Meduim */
#define SD16BUF_3           (3<<13)             /* SD16 High Imp. Input Buffer: Fast */
#endif

#if !defined(__msp430_have_sd16a)
#define AFEINCTL0           SD16INCTL0          /* SD16 Input Control Register Channel 0 */
#define AFEINCTL1           SD16INCTL1          /* SD16 Input Control Register Channel 1 */
#define AFEINCTL2           SD16INCTL2          /* SD16 Input Control Register Channel 2 */
#define AFECTL              SD16CTL             /* SD16 Control Register */
#define AFECCTL0            SD16CCTL0           /* SD16 Channel 0 Control Register */
#define AFECCTL1            SD16CCTL1           /* SD16 Channel 1 Control Register */
#define AFECCTL2            SD16CCTL02          /* SD16 Channel 2 Control Register */
#endif


/* Aliases by mspgcc */
#define SD16DIV_DIV1        SD16DIV_0           /* SD16 Clock Divider Select /1 */
#define SD16DIV_DIV2        SD16DIV_1           /* SD16 Clock Divider Select /2 */
#define SD16DIV_DIV4        SD16DIV_2           /* SD16 Clock Divider Select /4 */
#define SD16DIV_DIV8        SD16DIV_3           /* SD16 Clock Divider Select /8 */

#if defined(__msp430_have_sd16a)       
#define SD16XDIV_DIV1       SD16XDIV_0          /* SD16 2.Clock Divider Select /1 */
#define SD16XDIV_DIV2       SD16XDIV_1          /* SD16 2.Clock Divider Select /3 */
#define SD16XDIV_DIV4       SD16XDIV_2          /* SD16 2.Clock Divider Select /16 */
#define SD16XDIV_DIV8       SD16XDIV_3          /* SD16 2.Clock Divider Select /48 */
#endif

#define SD16SSEL_MCLK       SD16SSEL_0          /* AFE Clock Source Select MCLK  */
#define SD16SSEL_SMCLK      SD16SSEL_1          /* AFE Clock Source Select SMCLK */
#define SD16SSEL_ACLK       SD16SSEL_2          /* AFE Clock Source Select ACLK  */
#define SD16SSEL_TACLK      SD16SSEL_3          /* AFE Clock Source Select TACLK */


#endif
