/* References:
 * 
 * This file is a modified version of Demo.java taken from 
 * http://como.vub.ac.be/Robotics/wiki/index.php/Compiling,_uploading_and_interacting_with_Contiki:_Hello_World
 * 
 * Examples that are located under "contiki-2.4/examples" are also examined.
 */

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.util.Vector;

/*
 * Interfaces to/from tmote sensor:
 * write "extract_report_packet" to serialdump
 * get "a.b:x.y:fire:exit" from serialdump
 */
public class SerialCommunication {
	
	public static final String SERIALDUMP_LINUX = "../tools/sky/serialdump-linux";
	private Process serialDumpProcess;
	private PrintWriter serialOutput;
	private BufferedReader input;
	private Vector<String> data;
	private Vector<String> fireData;
	private boolean continueSetup;
	private Tellstick ts;	

	/**
	 * Constructor
	 * @param comPort /dev/ttyUSB0
	 */
	public SerialCommunication(String comPort, final String purpose) {
		ts = new Tellstick();
        ts.tdOpen();
		System.out.println("Listening on COM port: " + comPort);

	    /* Connect to COM using external serialdump application */
	    String fullCommand;
	    fullCommand = SERIALDUMP_LINUX + " " + "-b115200" + " " + comPort;
	    data = new Vector<String>();
	    fireData = new Vector<String>();
	    continueSetup = true;
	 
	    try {
		    String[] cmd = fullCommand.split(" ");
		    serialDumpProcess = Runtime.getRuntime().exec(cmd);
		    input = new BufferedReader(new InputStreamReader(
		    							serialDumpProcess.getInputStream()));
		    serialOutput = new PrintWriter(new OutputStreamWriter(
		    							serialDumpProcess.getOutputStream()));
	
		    /* Start thread listening on stdout */
		    Thread readInput = new Thread(new Runnable() {
			    public void run() {
				    if(purpose.equals("plotMap")){
				    	System.out.println("Plotting map");
				    	plotMap();
				    }
				    else if(purpose.equals("setUpSensor")){
				    	System.out.println("Setting up sensor");
				    	while(continueSetup){
				    		
				    	}
				    }
			    }
		    });	
		 
		    readInput.start();
	 
	    } catch (Exception e) {
	    	System.err.println("Exception when executing '" + fullCommand + "'");
	    	System.err.println("Exiting application");
	    	e.printStackTrace();
	    	System.exit(1);
	    }
	}
	
	/**
	 * Setter for boolean value of Setup functionality
	 * Used to start/stop setup cycle
	 */
	public void continueSetup(boolean flag){
		continueSetup = flag;
	}
	
	/**
	 * Write data on serialdump of sensor node
	 * @param data
	 */
	public void writeSerialData(String data) {
	    serialOutput.println(data);
	    serialOutput.flush();
	}
	
	/**
	 * Get stored data
	 * Data is retrieved from the network
	 * @return data
	 */
	public Vector<String> getData(){
		return data;
	}
	
	/**
	 * Get stored fire data
	 * fireData is retrieved from the network
	 * @return fireData
	 */
	public Vector<String> getFireData(){
		return fireData;
	}
	
	/**
	 * Clears fire announcement
	 * This function is used to clear fires that happen after GUI is started.
	 */
	public void cleanFire(int index){
		if(fireData.get(index).startsWith("clean:")){
			String temp = fireData.get(index);
			temp = temp.substring(temp.indexOf(':')+1);
			fireData.removeElementAt(index);
			for(int i=index-1; i>=0; i--){				
				if(fireData.get(i).equals(temp)){
					fireData.removeElementAt(i);
				}
			}
		}
	}
	
	/**
	 * Changes status of fire on a specific node
	 * It is required to clean fire from a node that is detected at the beginning of the GUI
	 */
	public void changeFireStatus(int index, String val){
		String string = data.get(index);
		//a.b:x.y:fire:exit
		int i = string.lastIndexOf(':');
		String element = string.substring(0, i-1) + val + string.substring(i);
		
		data.setElementAt(element, index);
	}

	/**
	 * Called when network is going to be plotted on the map
	 */
	public void plotMap(){
		int id = ts.tdGetDeviceId(0);
        int supportedMethods = Tellstick.TELLSTICK_TURNON | Tellstick.TELLSTICK_TURNOFF | Tellstick.TELLSTICK_LEARN;
        int methods = ts.tdMethods( id, supportedMethods );
		String line;
		// send command to sensor node
    	writeSerialData("sink");
    	writeSerialData("extract_report_packet");
    	
	    while(true) { // continuously read from the sensor node
	    	try {
				line = input.readLine();
				if(line != null){ 
		    		if(line.startsWith("@NODE_REPORT_PACKET:")){ // a.b:x.y:fire:exit
		    			int beginIndex = line.indexOf(':');
		    			line = line.substring(beginIndex+1);
		    			if(!data.contains(line)){
		    				data.addElement(line);
		    				System.out.println("@NODE_REPORT_PACKET:" + line);
		    			}
		    		}
		    		else if(line.startsWith("@EMERGENCY_PACKET:")){ // rimeID
		    			int beginIndex = line.indexOf(':');
		    			line = line.substring(beginIndex+1);
		    			if(!fireData.contains(line)){
		    				fireData.addElement(line);
		    				System.out.println("@EMERGENCY_PACKET:" + line);
		    			}
						// tellstick                                              
						if ( (methods & Tellstick.TELLSTICK_TURNON) != 0 ) {
							System.out.println( "The device supports tdTurnOn()");
							ts.tdTurnOn( id );                                            
						 }                                           
						 // end of tellstick
		    		}
		    		else if(line.startsWith("@ANTI_EMERGENCY_PACKET:")){ // rimeID
		    			int beginIndex = line.indexOf(':');
		    			line = line.substring(beginIndex+1);
		    			String cleanLine = "clean:"+line;
		    			if(!fireData.contains(cleanLine)){
		    				fireData.addElement(cleanLine);
		    				System.out.println("@ANTI_EMERGENCY_PACKET:" + line);
		    			}
						if ( (methods & Tellstick.TELLSTICK_TURNOFF) != 0 ) {
							System.out.println( "The device supports tdTurnOff()");
							ts.tdTurnOff(id);
						}
		    		}
		    	}
			} catch (IOException e) {
				System.err.println("Cannot read from sensor node");
				ts.tdClose();
			}
	    }
	}
}
