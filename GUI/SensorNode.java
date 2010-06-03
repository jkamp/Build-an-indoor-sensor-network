
public class SensorNode {

	private String rimeID;
	private int xCoordinate;
	private int yCoordinate;
	private boolean isOnFire;
	private boolean isExit;
	
	/**
	 * Constructor
	 * @param rimeID Rime ID of the sensor node
	 * @param xCoordinate Virtual x coordinate of the sensor node
	 * @param yCoordinate Virtual y coordinate of the sensor node
	 * @param isOnFire Is this sensor node on fire?
	 * @param isExit Is this sensor node placed on exit?
	 */
	public SensorNode(String rimeID, int xCoordinate, int yCoordinate,
									boolean isOnFire, boolean isExit) {
		
		this.rimeID = rimeID;
		this.xCoordinate = xCoordinate;
		this.yCoordinate = yCoordinate;
		this.isOnFire = isOnFire;
		this.isExit = isExit;
	}	

	/**
	 * Checks equality of two sensor node objects
	 * @return boolean true if two objects have the same content, otherwise false
	 */
	@Override
	public boolean equals(Object obj) {
		SensorNode arg = (SensorNode) obj;
		if( this.rimeID.equals(arg.rimeID) &&
			this.xCoordinate == arg.xCoordinate &&
			this.yCoordinate == arg.yCoordinate &&
			this.isOnFire == arg.isOnFire &&
			this.isExit == arg.isExit){
			
			return true;
		}
		else{
			return false;
		}
	}

	/**
	 * Getter of Rime ID
	 * @return rimeID
	 */
	public String getRimeID() {
		return rimeID;
	}
	
	/**
	 * Getter of x coordinate
	 * @return xCoordinate
	 */
	public int getxCoordinate() {
		return xCoordinate;
	}

	/**
	 * Getter of y coordinate
	 * @return yCoordinate
	 */
	public int getyCoordinate() {
		return yCoordinate;
	}

	/**
	 * Getter of fire status
	 * @return isOnFire true if on fire, otherwise false
	 */
	public boolean isOnFire() {
		return isOnFire;
	}

	/**
	 * Setter of fire status
	 * @param setOnFire
	 */
	public void setOnFire(boolean isOnFire) {
		this.isOnFire = isOnFire;
	}

	/**
	 * Getter of exit status
	 * @return isExit true if exit node, otherwise false
	 */
	public boolean isExit() {
		return isExit;
	}
	
	/**
	 * Prints corresponding String value of this sensor node
	 * @return String Sensor info is output in "A.B, (X,Y), Z, W" format without quotes
	 */
	public String toString(){
		String s = rimeID + ", (" + xCoordinate + "," + yCoordinate + "), " + isOnFire + ", " + isExit;
		return s;
	}
	
	/**
	 * Parse sensor node from String 
	 * Format of the String is "a.b:x.y:fire:exit" without quotes
	 * @param nodeData String value
	 * @return SensorNode parsed object from String
	 */
	public static SensorNode parseSensorNode(String nodeData){
		String s = nodeData;
		
		// Parse Rime ID
		int index = s.indexOf(':');
		String rime = s.substring(0, index);
		
		// Parse x coordinate
		s = s.substring(index+1);
		index = s.indexOf('.');
		int x = Integer.parseInt(s.substring(0, index));

		// Parse y coordinate
		s = s.substring(index+1);
		index = s.indexOf(':');
		int y = Integer.parseInt(s.substring(0, index));
		
		// Parse fire
		s = s.substring(index+1);
		index = s.indexOf(':');		
		boolean fire;		
		if(s.substring(0, index).equals("0")){
			fire = false;
		}
		else{
			fire = true;
		}
		
		// Parse exit
		s = s.substring(index+1);
		boolean exit;
		if(s.equals("0")){
			exit = false;
		}
		else{
			exit = true;
		}
		
		return new SensorNode(rime,x,y,fire,exit);
	}
}
