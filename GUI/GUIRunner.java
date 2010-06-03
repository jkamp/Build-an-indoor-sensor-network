
/**
 * GUIRunner is a driver class of SensorNetworkGUI.
 * This is the main class to run the user interface
 * @author volkanc
 *
 */
public class GUIRunner extends Thread {
	
	private SensorNetworkGUI gui;
	private static SerialCommunication serialCom;
	
	/**
	 * Run method is executed when GUIRunner thread is started
	 */
	@Override
	public void run() {
		if(serialCom == null){
			System.out.println("Creating serial communication");
			serialCom = new SerialCommunication("/dev/ttyUSB0","plotMap");
		}
		gui = new SensorNetworkGUI(serialCom);
	}
	
	/**
	 * Getter of SensorNetworkGUI
	 * @return gui
	 */
	public SensorNetworkGUI getGui() {
		return gui;
	}

	/**
	 * @param args
	 */
	public static void main(String[] args) {
		// Start user interface
		GUIRunner guiRunner = new GUIRunner();
		guiRunner.start();
		
		// Continuously check if there is new data to plot on the user interface
		while(true){
			SensorNetworkGUI snGUI = guiRunner.getGui();
			
			if(snGUI!=null && snGUI.checkNewData()==true){
				snGUI.terminate();				
				guiRunner = new GUIRunner();
				guiRunner.start();	
			}
		}
	}
}
