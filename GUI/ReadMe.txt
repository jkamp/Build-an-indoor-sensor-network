This is the latest version of the GUI. I didn't remove the
debugging-purpose-println statements. 

In order to use the GUI:

Plug in the sensor, make sure it is on /dev/ttyUSB0. If it has another name,
then "/dev/ttyUSB0" must be updated in java code.
javac GUIRunner.java
javac SetupSensorGUI.java

If you want to set a sensor then run "java SetupSensorGUI". Before hitting
setup button in GUI, you need to hit user button of the sensor node that you
want to set.
If you want to plot the network then run "java GUIRunner". There are some files
related to tellstick. These files are integrated into the SerialCommunication.
If java code breaks while running, it is probably because of tellstick. It may
be because of the absence of the tellstick. This thing is not tested by me

Volkan 
