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
 * $Id: ContikiRadio.java,v 1.31 2010/02/05 09:07:13 fros4943 Exp $
 */

package se.sics.cooja.contikimote.interfaces;

import java.awt.BorderLayout;
import java.awt.GridLayout;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.util.Collection;
import java.util.Observable;
import java.util.Observer;

import javax.swing.JButton;
import javax.swing.JLabel;
import javax.swing.JPanel;

import org.apache.log4j.Logger;
import org.jdom.Element;

import se.sics.cooja.COOJARadioPacket;
import se.sics.cooja.Mote;
import se.sics.cooja.RadioPacket;
import se.sics.cooja.SectionMoteMemory;
import se.sics.cooja.Simulation;
import se.sics.cooja.contikimote.ContikiMote;
import se.sics.cooja.contikimote.ContikiMoteInterface;
import se.sics.cooja.interfaces.PolledAfterActiveTicks;
import se.sics.cooja.interfaces.Position;
import se.sics.cooja.interfaces.Radio;
import se.sics.cooja.radiomediums.UDGM;

/**
 * Packet radio transceiver mote interface.
 *
 * To simulate transmission rates, the underlying Contiki system is
 * locked in TX or RX states using multi-threading library.
 *
 * Contiki variables:
 * <ul>
 * <li>char simTransmitting (1=mote radio is transmitting)
 * <li>char simReceiving (1=mote radio is receiving)
 * <li>char simInPolled
 * <p>
 * <li>int simInSize (size of received data packet)
 * <li>byte[] simInDataBuffer (data of received data packet)
 * <p>
 * <li>int simOutSize (size of transmitted data packet)
 * <li>byte[] simOutDataBuffer (data of transmitted data packet)
 * <p>
 * <li>char simRadioHWOn (radio hardware status (on/off))
 * <li>int simSignalStrength (heard radio signal strength)
 * <li>int simLastSignalStrength
 * <li>char simPower (number indicating power output)
 * <li>int simRadioChannel (number indicating current channel)
 * </ul>
 * <p>
 *
 * Core interface:
 * <ul>
 * <li>radio_interface
 * </ul>
 * <p>
 *
 * This observable notifies at radio state changes during RX and TX.
 *
 * @see #getLastEvent()
 * @see UDGM
 *
 * @author Fredrik Osterlind
 */
public class ContikiRadio extends Radio implements ContikiMoteInterface, PolledAfterActiveTicks {
  private ContikiMote mote;

  private SectionMoteMemory myMoteMemory;

  private static Logger logger = Logger.getLogger(ContikiRadio.class);

  /**
   * Transmission bitrate (kbps).
   */
  public final double RADIO_TRANSMISSION_RATE_kbps;

  private RadioPacket packetToMote = null;

  private RadioPacket packetFromMote = null;

  private boolean radioOn = true;

  private boolean isTransmitting = false;

  private boolean isInterfered = false;

  private long transmissionEndTime = -1;

  private RadioEvent lastEvent = RadioEvent.UNKNOWN;

  private long lastEventTime = 0;

  private int oldOutputPowerIndicator = -1;

  /**
   * Creates an interface to the radio at mote.
   *
   * @param mote Mote
   *
   * @see Mote
   * @see se.sics.cooja.MoteInterfaceHandler
   */
  public ContikiRadio(Mote mote) {
    // Read class configurations of this mote type
    RADIO_TRANSMISSION_RATE_kbps = mote.getType().getConfig().getDoubleValue(
        ContikiRadio.class, "RADIO_TRANSMISSION_RATE_kbps");

    this.mote = (ContikiMote) mote;
    this.myMoteMemory = (SectionMoteMemory) mote.getMemory();

    radioOn = myMoteMemory.getByteValueOf("simRadioHWOn") == 1;
  }

  /* Contiki mote interface support */
  public static String[] getCoreInterfaceDependencies() {
    return new String[] { "radio_interface" };
  }

  /* Packet radio support */
  public RadioPacket getLastPacketTransmitted() {
    return packetFromMote;
  }

  public RadioPacket getLastPacketReceived() {
    return packetToMote;
  }

  public void setReceivedPacket(RadioPacket packet) {
    packetToMote = packet;
  }

  /* General radio support */
  public boolean isReceiverOn() {
    return radioOn;
  }

  public boolean isTransmitting() {
    return isTransmitting;
  }

  public boolean isReceiving() {
    return isLockedAtReceiving();
  }

  public boolean isInterfered() {
    return isInterfered;
  }

  public int getChannel() {
    return myMoteMemory.getIntValueOf("simRadioChannel");
  }

  public void signalReceptionStart() {
    packetToMote = null;
    if (isInterfered() || isReceiving() || isTransmitting()) {
      interfereAnyReception();
      return;
    }
    lockInReceivingMode();

    lastEventTime = mote.getSimulation().getSimulationTime();
    lastEvent = RadioEvent.RECEPTION_STARTED;
    this.setChanged();
    this.notifyObservers();
  }

  public void signalReceptionEnd() {
    if (isInterfered() || packetToMote == null) {
      // Reset interfered flag
      isInterfered = false;

      // Reset data
      packetToMote = null;
      myMoteMemory.setIntValueOf("simInSize", 0);

      // Unlock (if locked)
      myMoteMemory.setByteValueOf("simReceiving", (byte) 0);

      mote.requestImmediateWakeup();

      lastEventTime = mote.getSimulation().getSimulationTime();
      lastEvent = RadioEvent.RECEPTION_FINISHED;
      this.setChanged();
      this.notifyObservers();
      return;
    }

    // Unlock (if locked)
    myMoteMemory.setByteValueOf("simReceiving", (byte) 0);

    // Set data
    myMoteMemory.setIntValueOf("simInSize", packetToMote.getPacketData().length);
    myMoteMemory.setByteArray("simInDataBuffer", packetToMote.getPacketData());

    lastEventTime = mote.getSimulation().getSimulationTime();
    lastEvent = RadioEvent.RECEPTION_FINISHED;
    mote.requestImmediateWakeup();
    this.setChanged();
    this.notifyObservers();
  }

  public RadioEvent getLastEvent() {
    return lastEvent;
  }

  public void interfereAnyReception() {
    if (!isInterfered()) {
      isInterfered = true;

      lastEvent = RadioEvent.RECEPTION_INTERFERED;
      lastEventTime = mote.getSimulation().getSimulationTime();
      this.setChanged();
      this.notifyObservers();
    }
  }

  public double getCurrentOutputPower() {
    // TODO Implement method
    logger.warn("Not implemented, always returning 0 dBm");
    return 0;
  }

  public int getOutputPowerIndicatorMax() {
    return 100;
  }

  public int getCurrentOutputPowerIndicator() {
    return myMoteMemory.getByteValueOf("simPower");
  }

  public double getCurrentSignalStrength() {
    return myMoteMemory.getIntValueOf("simSignalStrength");
  }

  public void setCurrentSignalStrength(double signalStrength) {
    myMoteMemory.setIntValueOf("simSignalStrength", (int) signalStrength);
  }

  public Position getPosition() {
    return mote.getInterfaces().getPosition();
  }

  /**
   * @return True if locked at transmitting
   */
  private boolean isLockedAtTransmitting() {
    return myMoteMemory.getByteValueOf("simTransmitting") == 1;
  }

  /**
   * @return True if locked at receiving
   */
  private boolean isLockedAtReceiving() {
    return myMoteMemory.getByteValueOf("simReceiving") == 1;
  }

  /**
   * Locks underlying Contiki system in receiving mode. This may, but does not
   * have to, be used during a simulated data transfer that takes longer than
   * one tick to complete. The system is unlocked by delivering the received
   * data to the mote.
   */
  private void lockInReceivingMode() {
    mote.requestImmediateWakeup();

    // Lock core radio in receiving loop
    myMoteMemory.setByteValueOf("simReceiving", (byte) 1);
  }

  public void doActionsAfterTick() {
    /* Check if radio hardware status changed */
    if (radioOn != (myMoteMemory.getByteValueOf("simRadioHWOn") == 1)) {
      radioOn = !radioOn;

      if (!radioOn) {
        // Reset status
        myMoteMemory.setByteValueOf("simReceiving", (byte) 0);
        myMoteMemory.setIntValueOf("simInSize", 0);
        myMoteMemory.setByteValueOf("simTransmitting", (byte) 0);
        myMoteMemory.setIntValueOf("simOutSize", 0);
        isTransmitting = false;
        lastEvent = RadioEvent.HW_OFF;
      } else {
        lastEvent = RadioEvent.HW_ON;
      }

      lastEventTime = mote.getSimulation().getSimulationTime();
      this.setChanged();
      this.notifyObservers();
    }
    if (!radioOn) {
      return;
    }

    // Check if radio output power changed
    if (myMoteMemory.getByteValueOf("simPower") != oldOutputPowerIndicator) {
      oldOutputPowerIndicator = myMoteMemory.getByteValueOf("simPower");
      lastEvent = RadioEvent.UNKNOWN;
      this.setChanged();
      this.notifyObservers();
    }

    /* Are we transmitting but should stop? */
    /* TODO Use time events */
    if (isTransmitting
        && mote.getSimulation().getSimulationTime() >= transmissionEndTime) {
      myMoteMemory.setByteValueOf("simTransmitting", (byte) 0);
      myMoteMemory.setIntValueOf("simOutSize", 0);
      isTransmitting = false;

      lastEventTime = mote.getSimulation().getSimulationTime();
      lastEvent = RadioEvent.TRANSMISSION_FINISHED;
      // TODO Energy consumption of transmitted packet?
      this.setChanged();
      this.notifyObservers();
      //logger.debug("----- CONTIKI TRANSMISSION ENDED -----");
    }

    // Check if a new transmission should be started
    if (!isTransmitting && myMoteMemory.getByteValueOf("simTransmitting") == 1) {
      int size = myMoteMemory.getIntValueOf("simOutSize");
      if (size <= 0) {
        logger.warn("Skipping zero sized Contiki packet (no size)");
        myMoteMemory.setByteValueOf("simTransmitting", (byte) 0);
        return;
      }
      packetFromMote = new COOJARadioPacket(myMoteMemory.getByteArray("simOutDataBuffer", size));
      if (packetFromMote.getPacketData() == null || packetFromMote.getPacketData().length == 0) {
        logger.warn("Skipping zero sized Contiki packet (no buffer)");
        myMoteMemory.setByteValueOf("simTransmitting", (byte) 0);
        return;
      }

      isTransmitting = true;

      /* Calculate transmission duration (us) */
      /* XXX Currently floored due to millisecond scheduling! */
      long duration = (int) (Simulation.MILLISECOND*((8 * size /*bits*/) / RADIO_TRANSMISSION_RATE_kbps));
      transmissionEndTime = mote.getSimulation().getSimulationTime() + Math.max(1, duration);
      lastEventTime = mote.getSimulation().getSimulationTime();

      lastEvent = RadioEvent.TRANSMISSION_STARTED;
      this.setChanged();
      this.notifyObservers();
      //logger.debug("----- NEW CONTIKI TRANSMISSION DETECTED -----");

      // Deliver packet right away
      lastEvent = RadioEvent.PACKET_TRANSMITTED;
      this.setChanged();
      this.notifyObservers();
      //logger.debug("----- CONTIKI PACKET DELIVERED -----");
    }
  }

  public JPanel getInterfaceVisualizer() {
    // Location
    JPanel wrapperPanel = new JPanel(new BorderLayout());
    JPanel panel = new JPanel(new GridLayout(5, 2));

    final JLabel statusLabel = new JLabel("");
    final JLabel lastEventLabel = new JLabel("");
    final JLabel channelLabel = new JLabel("");
    final JLabel powerLabel = new JLabel("");
    final JLabel ssLabel = new JLabel("");
    final JButton updateButton = new JButton("Update");

    panel.add(new JLabel("STATE:"));
    panel.add(statusLabel);

    panel.add(new JLabel("LAST EVENT:"));
    panel.add(lastEventLabel);

    panel.add(new JLabel("CHANNEL:"));
    panel.add(channelLabel);

    panel.add(new JLabel("OUTPUT POWER:"));
    panel.add(powerLabel);

    panel.add(new JLabel("SIGNAL STRENGTH:"));
    JPanel smallPanel = new JPanel(new GridLayout(1, 2));
    smallPanel.add(ssLabel);
    smallPanel.add(updateButton);
    panel.add(smallPanel);

    updateButton.addActionListener(new ActionListener() {
      public void actionPerformed(ActionEvent e) {
        channelLabel.setText("" + getChannel());
        powerLabel.setText(getCurrentOutputPower() + " dBm (indicator=" + getCurrentOutputPowerIndicator() + "/" + getOutputPowerIndicatorMax() + ")");
        ssLabel.setText(getCurrentSignalStrength() + " dBm");
      }
    });

    Observer observer;
    this.addObserver(observer = new Observer() {
      public void update(Observable obs, Object obj) {
        if (isTransmitting()) {
          statusLabel.setText("transmitting");
        } else if (isReceiving()) {
          statusLabel.setText("receiving");
        } else if (radioOn) {
          statusLabel.setText("listening for traffic");
        } else {
          statusLabel.setText("HW off");
        }

        lastEventLabel.setText(lastEvent + " @ time=" + lastEventTime);

        channelLabel.setText("" + getChannel());
        powerLabel.setText(getCurrentOutputPower() + " dBm (indicator=" + getCurrentOutputPowerIndicator() + "/" + getOutputPowerIndicatorMax() + ")");
        ssLabel.setText(getCurrentSignalStrength() + " dBm");
      }
    });

    observer.update(null, null);

    wrapperPanel.add(BorderLayout.NORTH, panel);

    // Saving observer reference for releaseInterfaceVisualizer
    wrapperPanel.putClientProperty("intf_obs", observer);
    return wrapperPanel;
  }

  public void releaseInterfaceVisualizer(JPanel panel) {
    Observer observer = (Observer) panel.getClientProperty("intf_obs");
    if (observer == null) {
      logger.fatal("Error when releasing panel, observer is null");
      return;
    }

    this.deleteObserver(observer);
  }

  public Collection<Element> getConfigXML() {
    return null;
  }

  public void setConfigXML(Collection<Element> configXML, boolean visAvailable) {
  }

  public Mote getMote() {
    return mote;
  }

  public String toString() {
    return "Radio at " + mote;
  }
}
