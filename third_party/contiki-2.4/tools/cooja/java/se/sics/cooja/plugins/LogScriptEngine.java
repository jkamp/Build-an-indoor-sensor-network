/*
 * Copyright (c) 2009, Swedish Institute of Computer Science.
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
 * $Id: LogScriptEngine.java,v 1.22 2010/02/03 09:32:25 fros4943 Exp $
 */

package se.sics.cooja.plugins;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.lang.reflect.UndeclaredThrowableException;
import java.util.Hashtable;
import java.util.Observable;
import java.util.Observer;
import java.util.concurrent.Semaphore;

import javax.script.Invocable;
import javax.script.ScriptEngine;
import javax.script.ScriptEngineManager;
import javax.script.ScriptException;

import org.apache.log4j.Logger;

import se.sics.cooja.GUI;
import se.sics.cooja.Mote;
import se.sics.cooja.Simulation;
import se.sics.cooja.TimeEvent;
import se.sics.cooja.SimEventCentral.MoteCountListener;

/**
 * Loads and executes a Contiki test script.
 * A Contiki test script is a Javascript that depends on a single simulation,
 * and reacts to mote log output (such as printf()s).
 *
 * @see ScriptRunner
 * @author Fredrik Osterlind
 */
public class LogScriptEngine {
  private static Logger logger = Logger.getLogger(LogScriptEngine.class);
  private static final long DEFAULT_TIMEOUT = 20*60*1000*Simulation.MILLISECOND; /* 1200s = 20 minutes */

  private ScriptEngine engine = 
    new ScriptEngineManager().getEngineByName("JavaScript");

  /* Log observer: watches all log interfaces */
  private Observer logObserver = new Observer() {
    public void update(Observable obs, Object obj) {
      Mote mote = (Mote) obj;
      handleNewMoteOutput(
          mote,
          mote.getID(),
          mote.getSimulation().getSimulationTime(),
          mote.getInterfaces().getLog().getLastLogMessage()
      );
    }
  };
  
  /* Mote count observer: keeps track of newly added nodes */
  private MoteCountListener newMotesListener = new MoteCountListener() {
    public void moteWasAdded(Mote mote) {
      if (mote.getInterfaces().getLog() != null) {
        mote.getInterfaces().getLog().addObserver(logObserver);
      }
    }
    public void moteWasRemoved(Mote mote) {
      if (mote.getInterfaces().getLog() != null) {
        mote.getInterfaces().getLog().deleteObserver(logObserver);
      }
    }
  };

  private Semaphore semaphoreScript = null; /* Semaphores blocking script/simulation */
  private Semaphore semaphoreSim = null;
  private Thread scriptThread = null; /* Script thread */
  private Observer scriptLogObserver = null;
  private ScriptMote scriptMote;

  private boolean stopSimulation = false, quitCooja = false;

  private Simulation simulation;

  private boolean scriptActive = false;

  private interface ScriptLog {
    public void log(String log);
    public void testOK();
    public void testFailed();
    public void generateMessage(long delay, String msg);
  }

  /* Only called from the simulation loop */
  private void stepScript() {
    /* Release script - halt simulation */
    semaphoreScript.release();

    /* ... script executing ... */

    try {
      semaphoreSim.acquire();
    } catch (InterruptedException e1) {
      e1.printStackTrace();
    }

    /* ... script is now again waiting for script semaphore ... */

    /* Check if test script requested us to stop */
    if (stopSimulation) {
      stopSimulationRunnable.run();
      stopSimulation = false;
    }
    if (quitCooja) {
      quitRunnable.run();
      quitCooja = false;
    }
  }

  public LogScriptEngine(Simulation simulation) {
    this.simulation = simulation;
  }

  /* Only called from the simulation loop */
  private void handleNewMoteOutput(Mote mote, int id, long time, String msg) {
    try {
      if (scriptThread == null ||
          !scriptThread.isAlive()) {
        logger.warn("No script thread, deactivate script.");
        /*scriptThread.isInterrupted()*/
        return;
      }

      /* Update script variables */
      engine.put("mote", mote);
      engine.put("id", id);
      engine.put("time", time);
      engine.put("msg", msg);

      stepScript();
    } catch (UndeclaredThrowableException e) {
      logger.fatal("Exception: " + e.getMessage(), e);
      if (GUI.isVisualized()) {
        GUI.showErrorDialog(GUI.getTopParentContainer(),
            e.getMessage(),
            e, false);
      }
      for (Mote m: simulation.getMotes()) {
        newMotesListener.moteWasRemoved(m);
      }
      simulation.stopSimulation();
    }
  }

  /**
   * Inject faked mote log output.
   * Should only be used for debugging!
   *
   * @param msg Log message
   * @param mote Mote
   */
  public void fakeMoteLogOutput(final String msg, final Mote mote) {
    simulation.invokeSimulationThread(new Runnable() {
      public void run() {
        handleNewMoteOutput(
            mote,
            mote.getID(),
            mote.getSimulation().getSimulationTime(),
            msg
        );
      }
    });
  }

  public void setScriptLogObserver(Observer observer) {
    scriptLogObserver = observer;
  }

  /**
   * Deactivate script
   */
  public void deactivateScript() {
    if (!scriptActive) {
      return;
    }
    scriptActive = false;

    if (semaphoreScript == null) {
      /*logger.warn("semaphoreScript is not initialized");*/
    }
    if (semaphoreSim == null) {
      /*logger.warn("semaphoreSim is not initialized");*/
    }
    if (scriptThread == null) {
      /*logger.warn("scriptThread is not initialized");*/
    }

    timeoutEvent.remove();

    simulation.getEventCentral().removeMoteCountListener(newMotesListener);
    for (Mote mote: simulation.getMotes()) {
      newMotesListener.moteWasRemoved(mote);
    }

    engine.put("SHUTDOWN", true);

    try {
      semaphoreScript.release(100);
    } catch (Exception e) {
    } finally {
      semaphoreScript = null;
    }
    try {
      semaphoreSim.release(100);
    } catch (Exception e) {
    } finally {
      semaphoreSim = null;
    }

    if (scriptThread != null && 
        scriptThread != Thread.currentThread() /* XXX May deadlock */ ) {
      try {
        scriptThread.join();
      } catch (InterruptedException e) {
        e.printStackTrace();
      } finally {
      }
    }
    scriptThread = null;

  }

  public void activateScript(String scriptCode) throws ScriptException {
    if (scriptActive) {
      return;
    }
    scriptActive = true;

    if (semaphoreScript != null) {
      logger.warn("semaphoreScript is already initialized");
    }
    if (semaphoreSim != null) {
      logger.warn("semaphoreSim is already initialized");
    }
    if (scriptThread != null) {
      logger.warn("scriptThread is already initialized");
    }

    /* Parse current script */
    ScriptParser parser = new ScriptParser(scriptCode);
    String jsCode = parser.getJSCode();

    long timeoutTime = parser.getTimeoutTime();
    if (timeoutTime < 0) {
      logger.info("No timeout defined, using default (us): " + DEFAULT_TIMEOUT);
      timeoutTime = DEFAULT_TIMEOUT;
    }
    final long absoluteTimeout = simulation.getSimulationTime() + timeoutTime;
    simulation.invokeSimulationThread(new Runnable() {
      public void run() {
        simulation.scheduleEvent(timeoutEvent, absoluteTimeout);
      }
    });

    engine.eval(jsCode);

    /* Setup script control */
    semaphoreScript = new Semaphore(1);
    semaphoreSim = new Semaphore(1);
    engine.put("TIMEOUT", false);
    engine.put("SHUTDOWN", false);
    engine.put("SEMAPHORE_SCRIPT", semaphoreScript);
    engine.put("SEMAPHORE_SIM", semaphoreSim);

    try {
      semaphoreScript.acquire();
    } catch (InterruptedException e) {
      logger.fatal("Error when creating engine: " + e.getMessage(), e);
    }
    ThreadGroup group = new ThreadGroup("script") {
      public void uncaughtException(Thread t, Throwable e) {
        while (e.getCause() != null) {
          e = e.getCause();
        }
        if (e.getMessage() != null &&
            e.getMessage().contains("test script killed") ) {
          /* Ignore normal shutdown exceptions */
        } else {
          logger.fatal("Script error:", e);
        }
      }
    };
    scriptThread = new Thread(group, new Runnable() {
      public void run() {
        /*logger.info("test script thread starts");*/
        try {
          ((Invocable)engine).getInterface(Runnable.class).run();
        } catch (RuntimeException e) {
          Throwable throwable = e;
          while (throwable.getCause() != null) {
            throwable = throwable.getCause();
          }

          if (throwable.getMessage() != null &&
              throwable.getMessage().contains("test script killed") ) {
            logger.info("Test script finished");
          } else {
            if (!GUI.isVisualized()) {
              logger.fatal("Test script error, terminating Cooja.");
              logger.fatal("Script error:", e);
              System.exit(1);
            }

            logger.fatal("Script error:", e);
            deactivateScript();
            simulation.stopSimulation();
            if (GUI.isVisualized()) {
              GUI.showErrorDialog(GUI.getTopParentContainer(),
                  "Script error", e, false);
            }
          }
        }
        /*logger.info("test script thread exits");*/
      }
    });
    scriptThread.start(); /* Starts by acquiring semaphore (blocks) */
    while (!semaphoreScript.hasQueuedThreads()) {
      Thread.yield();
    }

    /* Setup simulation observers */
    simulation.getEventCentral().addMoteCountListener(newMotesListener);
    for (Mote mote: simulation.getMotes()) {
      newMotesListener.moteWasAdded(mote);
    }

    /* Create script output logger */
    engine.put("log", new ScriptLog() {
      public void log(String msg) {
        if (scriptLogObserver != null) {
          scriptLogObserver.update(null, msg);
        }
      }
      public void append(String filename, String msg) {
        try{
          FileWriter fstream = new FileWriter(filename, true);
          BufferedWriter out = new BufferedWriter(fstream);
          out.write(msg);
          out.close();
        } catch (Exception e) {
          logger.warn("Test append failed: " + filename + ": " + e.getMessage());
        }
      }
      public void testOK() {
        log("TEST OK\n");

        if (GUI.isVisualized()) {
          log("[if test was run without visualization, COOJA would now have been terminated]\n");
          stopSimulation = true;
          simulation.invokeSimulationThread(stopSimulationRunnable);
        } else {
          quitCooja = true;
          simulation.invokeSimulationThread(quitRunnable);
        }

        timeoutEvent.remove();

        semaphoreSim.release(100);
        throw new RuntimeException("test script killed");
      }
      public void testFailed() {
        log("TEST FAILED\n");

        if (GUI.isVisualized()) {
          log("[if test was run without visualization, COOJA would now have been terminated]\n");
          stopSimulation = true;
          simulation.invokeSimulationThread(stopSimulationRunnable);
        } else {
          quitCooja = true;
          simulation.invokeSimulationThread(quitRunnable);
        }

        semaphoreSim.release(100);
        throw new RuntimeException("test script killed");
      }

      public void generateMessage(final long delay, final String msg) {
        final Mote currentMote = (Mote) engine.get("mote");
        final TimeEvent generateEvent = new TimeEvent(0) {
          public void execute(long t) {
            if (scriptThread == null ||
                !scriptThread.isAlive()) {
              logger.info("script thread not alive. try deactivating script.");
              /*scriptThread.isInterrupted()*/
              return;
            }

            /* Update script variables */
            engine.put("mote", currentMote);
            engine.put("id", currentMote.getID());
            engine.put("time", currentMote.getSimulation().getSimulationTime());
            engine.put("msg", msg);

            stepScript();
          }
        };
        simulation.invokeSimulationThread(new Runnable() {
          public void run() {
            simulation.scheduleEvent(
                generateEvent,
                simulation.getSimulationTime() + delay*Simulation.MILLISECOND);
          }
        });
      }
    });

    Hashtable<Object, Object> hash = new Hashtable<Object, Object>();
    engine.put("global", hash);

    scriptMote = new ScriptMote();
    engine.put("node", scriptMote);
  }

  private TimeEvent timeoutEvent = new TimeEvent(0) {
    public void execute(long t) {
      logger.info("Timeout event @ " + t);
      engine.put("TIMEOUT", true);
      stepScript();
    }
  };
  private Runnable stopSimulationRunnable = new Runnable() {
    public void run() {
      simulation.stopSimulation();
      timeoutEvent.remove();
    }
  };
  private Runnable quitRunnable = new Runnable() {
    public void run() {
      simulation.stopSimulation();
      new Thread() {
        public void run() {
          try { Thread.sleep(500); } catch (InterruptedException e) { }
          simulation.getGUI().doQuit(false);
        };
      }.start();
      new Thread() {
        public void run() {
          try { Thread.sleep(2000); } catch (InterruptedException e) { }
          logger.warn("Killing COOJA");
          System.exit(1);
        };
      }.start();
    }
  };

}
