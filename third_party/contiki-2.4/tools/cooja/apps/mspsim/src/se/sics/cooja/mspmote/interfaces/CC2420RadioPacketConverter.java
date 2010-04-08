/*
 * Copyright (c) 2008, Swedish Institute of Computer Science.
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
 * $Id: CC2420RadioPacketConverter.java,v 1.13 2010/02/03 14:07:45 fros4943 Exp $
 */

package se.sics.cooja.mspmote.interfaces;

import org.apache.log4j.Logger;

import se.sics.cooja.ConvertedRadioPacket;
import se.sics.cooja.RadioPacket;
import se.sics.mspsim.util.CCITT_CRC;

/**
 * Converts radio packets between nullmac/CC24240/Sky and COOJA.
 * Handles radio driver specifics such as length header and CRC footer.
 *
 * @author Fredrik Osterlind
 */
public class CC2420RadioPacketConverter {
  private static Logger logger = Logger.getLogger(CC2420RadioPacketConverter.class);

  public static final boolean WITH_PREAMBLE = true;
  public static final boolean WITH_SYNCH = true;
  public static final boolean WITH_XMAC = false; /* XXX No longer supported. Cross-level requires NULLMAC */
  public static final boolean WITH_CHECKSUM = false; /* Contiki checksum. Not CC2420's built-in. */
  public static final boolean WITH_TIMESTAMP = true;
  public static final boolean WITH_FOOTER = true; /* CC2420's checksum */

  public static byte[] fromCoojaToCC2420(RadioPacket packet) {
    byte cc2420Data[] = new byte[6+127];
    int pos = 0;
    byte packetData[] = packet.getPacketData();
    byte len; /* total packet minus preamble(4), synch(1) and length(1) */
    WrapperCC2420CRC cc2420CRC = new WrapperCC2420CRC();
    short contikiCRC = 0;

    /* 4 bytes preamble */
    if (WITH_PREAMBLE) {
      cc2420Data[pos++] = 0;
      cc2420Data[pos++] = 0;
      cc2420Data[pos++] = 0;
      cc2420Data[pos++] = 0;
    }

    /* 1 byte synch */
    if (WITH_SYNCH) {
      cc2420Data[pos++] = 0x7A;
    }

    /* 1 byte length */
    len = (byte) packetData.length;
    if (WITH_XMAC) {
      len += 6;
    }
    if (WITH_CHECKSUM) {
      len += 2;
    }
    if (WITH_TIMESTAMP) {
      len += 3;
    }
    if (WITH_FOOTER) {
      len += 2;
    }
    cc2420Data[pos++] = len;
    cc2420CRC.setCRC(0);
    cc2420CRC.add(len); /* TODO Should length be included in CRC? */

    /* 4 byte X-MAC: not implemented */
    if (WITH_XMAC) {
      cc2420Data[pos++] = 1; /* TYPE_DATA */
      cc2420CRC.add(1);
      contikiCRC = CRCCoder.crc16Add((byte)0, contikiCRC);
      cc2420Data[pos++] = 0;
      cc2420CRC.add(0);
      contikiCRC = CRCCoder.crc16Add((byte)0, contikiCRC);
      cc2420Data[pos++] = 0; /* XXX sender: 0.0 */
      cc2420CRC.add(0);
      contikiCRC = CRCCoder.crc16Add((byte)0, contikiCRC);
      cc2420Data[pos++] = 0;
      cc2420CRC.add(0);
      contikiCRC = CRCCoder.crc16Add((byte)0, contikiCRC);
      cc2420Data[pos++] = 0; /* XXX receiver: 0.0 */
      cc2420CRC.add(0);
      contikiCRC = CRCCoder.crc16Add((byte)0, contikiCRC);
      cc2420Data[pos++] = 0;
      cc2420CRC.add(0);
      contikiCRC = CRCCoder.crc16Add((byte)0, contikiCRC);
    }

    /* Payload */
    for (byte b : packetData) {
      contikiCRC = CRCCoder.crc16Add(b, contikiCRC);
      cc2420CRC.add(b & 0xFF);
    }
    System.arraycopy(packetData, 0, cc2420Data, pos, packetData.length);
    pos += packetData.length;

    /* 2 bytes checksum */
    if (WITH_CHECKSUM) {
      cc2420Data[pos++] = (byte) (contikiCRC & 0xff);
      cc2420CRC.add(contikiCRC & 0xFF);
      cc2420Data[pos++] = (byte) ((contikiCRC >> 8) & 0xff);
      cc2420CRC.add((contikiCRC >> 8) & 0xFF);
    }

    /* (TODO) 3 bytes timestamp */
    if (WITH_TIMESTAMP) {
      cc2420Data[pos++] = 0;
      cc2420CRC.add(0);
      cc2420Data[pos++] = 0;
      cc2420CRC.add(0);
      cc2420Data[pos++] = 0;
      cc2420CRC.add(0);
    }

    /* 2 bytes footer: CC2420's CRC */
    if (WITH_FOOTER) {
      /* TODO XXX Workaround for (potential) CC2420 bug:
       * it excludes the last byte in the CRC */
      int crc = cc2420CRC.getPreviousCRC();
      cc2420Data[pos++] = (byte) ((crc >> 8) & 0xff);
      cc2420Data[pos++] = (byte) (crc & 0xff);
    }

    byte cc2420DataStripped[] = new byte[pos];
    System.arraycopy(cc2420Data, 0, cc2420DataStripped, 0, pos);

    /*logger.info("Data length: " + cc2420DataStripped.length);*/
    return cc2420DataStripped;
  }

  public static ConvertedRadioPacket fromCC2420ToCooja(byte[] data) {
    int pos = 0;
    int len; /* Payload */
    int originalLen;

    /* Use some CC2420/MAC specific field such as X-MAC response */

    /* (IGNORED) 4 bytes preamble */
    if (WITH_PREAMBLE) {
      pos += 4;
    }

    /* (IGNORED) 1 byte synch */
    if (WITH_SYNCH) {
      pos += 1;
    }

    /* 1 byte length */
    len = data[pos];
    originalLen = len;
    pos += 1;

    /* 4 byte X-MAC: not implemented */
    if (WITH_XMAC) {
      pos += 6;
      len -= 6;
    }

    /* (IGNORED) 2 bytes checksum */
    if (WITH_CHECKSUM) {
      len -= 2;
    }

    /* (IGNORED) 3 bytes timestamp */
    if (WITH_TIMESTAMP) {
      len -= 3;
    }

    /* (IGNORED) 2 bytes footer */
    if (WITH_FOOTER) {
      /* TODO Implement */
      len -= 2;
    }

    /*logger.info("Payload pos: " + pos);
    logger.info("Payload length: " + len);*/

    byte originalData[] = new byte[originalLen];
    System.arraycopy(data, 6 /* skipping preamble+synch+len */, originalData, 0, originalLen);
    if (len < 0) {
      /*logger.warn("No cross-level conversion available: negative packet length");*/
      return new ConvertedRadioPacket(new byte[0], originalData);
    }
    byte convertedData[] = new byte[len];
    System.arraycopy(data, pos, convertedData, 0, len);
    return new ConvertedRadioPacket(convertedData, originalData);
  }

  private static class WrapperCC2420CRC extends CCITT_CRC {
    int previous = 0;
    public WrapperCC2420CRC() {
      super();
    }
    public int add(int data) {
      previous = getCRC();
      return super.add(data);
    }
    public int getPreviousCRC() {
      return previous;
    }
  }

}
