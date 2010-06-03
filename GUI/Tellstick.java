public class Tellstick{
        public static final int TELLSTICK_TURNON = 1;
        public static final int TELLSTICK_TURNOFF = 2;
        public static final int TELLSTICK_LEARN = 32;
        //public static final String path = "/home/user/exjobb/contiki-2.x/";
        public static final String path = "./";

        public native void tdOpen();
        public native void tdClose();
	public native boolean tdTurnOn(int intDeviceId);
	public native boolean tdTurnOff(int intDeviceId);
        public native int tdLearn(int intDeviceId);
	public native boolean tdBell(int intDeviceId);
	public native boolean tdDim(int intDeviceId, int level);
	public native int tdMethods(int id, int methodsSupported);

	public native int tdGetNumberOfDevices();
	public native int tdGetDeviceId(int intDeviceIndex);

	public native String tdGetName(int intDeviceId);
        public native boolean tdSetName(int intDeviceId, String chNewName);

        public native int tdAddDevice();
        public native boolean tdRemoveDevice(int intDeviceId);           
		
	static {
		System.load(path + "libts.so");
	}
}

