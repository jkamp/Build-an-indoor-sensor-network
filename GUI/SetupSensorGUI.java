/**
 *
 * @author volkanc
 */
public class SetupSensorGUI extends javax.swing.JFrame {

    private static final long serialVersionUID = -8539064638502178078L;
    
	/** Creates new form SetupSensorGUI */
    public SetupSensorGUI(SerialCommunication serial) {
    	serialCom = serial;
        initComponents();
    }

    private void initComponents() {

        rimeID = new javax.swing.JLabel();
        xCoordinate = new javax.swing.JLabel();
        yCoordinate = new javax.swing.JLabel();
        neighbors = new javax.swing.JLabel();
        isExit = new javax.swing.JRadioButton();
        setSensor = new javax.swing.JButton();
        rimeIDTextField = new javax.swing.JTextField();
        xCoordTextField = new javax.swing.JTextField();
        yCoordTextField = new javax.swing.JTextField();
        neighborsTextField = new javax.swing.JTextField();

        setDefaultCloseOperation(javax.swing.WindowConstants.EXIT_ON_CLOSE);

        rimeID.setText("Rime ID");

        xCoordinate.setText("X coordinate");

        yCoordinate.setText("Y coordinate");

        neighbors.setText("List of Neighbors");

        isExit.setText("Exit Node");

        setSensor.setText("Set sensor");
        setSensor.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                setSensorActionPerformed(evt);
            }
        });

        javax.swing.GroupLayout layout = new javax.swing.GroupLayout(getContentPane());
        getContentPane().setLayout(layout);
        layout.setHorizontalGroup(
            layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
            .addGroup(layout.createSequentialGroup()
                .addContainerGap()
                .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.CENTER)
                    .addComponent(setSensor)
                    .addComponent(isExit)
                    .addGroup(javax.swing.GroupLayout.Alignment.LEADING, layout.createSequentialGroup()
                        .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
                            .addComponent(neighbors)
                            .addComponent(yCoordinate)
                            .addComponent(xCoordinate)
                            .addComponent(rimeID))
                        .addGap(4, 4, 4)
                        .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
                            .addComponent(rimeIDTextField, javax.swing.GroupLayout.DEFAULT_SIZE, 126, Short.MAX_VALUE)
                            .addComponent(xCoordTextField, javax.swing.GroupLayout.DEFAULT_SIZE, 126, Short.MAX_VALUE)
                            .addComponent(yCoordTextField, javax.swing.GroupLayout.DEFAULT_SIZE, 126, Short.MAX_VALUE)
                            .addComponent(neighborsTextField, javax.swing.GroupLayout.DEFAULT_SIZE, 126, Short.MAX_VALUE))))
                .addContainerGap())
        );
        layout.setVerticalGroup(
            layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
            .addGroup(layout.createSequentialGroup()
                .addContainerGap()
                .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE)
                    .addComponent(rimeID)
                    .addComponent(rimeIDTextField, javax.swing.GroupLayout.PREFERRED_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.PREFERRED_SIZE))
                .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE)
                    .addComponent(xCoordinate)
                    .addComponent(xCoordTextField, javax.swing.GroupLayout.PREFERRED_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.PREFERRED_SIZE))
                .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE)
                    .addComponent(yCoordinate)
                    .addComponent(yCoordTextField, javax.swing.GroupLayout.PREFERRED_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.PREFERRED_SIZE))
                .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                .addGroup(layout.createParallelGroup(javax.swing.GroupLayout.Alignment.BASELINE)
                    .addComponent(neighbors)
                    .addComponent(neighborsTextField, javax.swing.GroupLayout.PREFERRED_SIZE, javax.swing.GroupLayout.DEFAULT_SIZE, javax.swing.GroupLayout.PREFERRED_SIZE))
                .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED)
                .addComponent(isExit)
                .addPreferredGap(javax.swing.LayoutStyle.ComponentPlacement.RELATED, javax.swing.GroupLayout.DEFAULT_SIZE, Short.MAX_VALUE)
                .addComponent(setSensor)
                .addContainerGap())
        );

        pack();
    }

    private void setSensorActionPerformed(java.awt.event.ActionEvent evt) {
        if(!rimeIDTextField.getText().equals("") &&
           !xCoordTextField.getText().equals("") &&
           !yCoordTextField.getText().equals("")){
            
            boolean exitNode = isExit.isSelected();
            String exit = "";
            if(exitNode){
                exit = "1";
            }
            else{
                exit = "0";
            }
            String result = "send_setup_packet:"+
            				rimeIDTextField.getText()+":"+
				            xCoordTextField.getText()+"."+
				            yCoordTextField.getText()+":"+
				            exit+":"+
				            neighborsTextField.getText();            
            System.out.println(result);
            serialCom.writeSerialData(result);
            serialCom.writeSerialData("send_init_packet");
			System.out.println("send_init_packet");
            serialCom.continueSetup(false);
            this.dispose();
        }
    }

    /**
    * @param args the command line arguments
    */
    public static void main(String args[]) {
        java.awt.EventQueue.invokeLater(new Runnable() {
            public void run() {
                new SetupSensorGUI(new SerialCommunication("/dev/ttyUSB0","setUpSensor")).setVisible(true);
            }
        });
    }
    
    private SerialCommunication serialCom;

    private javax.swing.JRadioButton isExit;
    private javax.swing.JLabel neighbors;
    private javax.swing.JTextField neighborsTextField;
    private javax.swing.JLabel rimeID;
    private javax.swing.JTextField rimeIDTextField;
    private javax.swing.JButton setSensor;
    private javax.swing.JTextField xCoordTextField;
    private javax.swing.JLabel xCoordinate;
    private javax.swing.JTextField yCoordTextField;
    private javax.swing.JLabel yCoordinate;

}
