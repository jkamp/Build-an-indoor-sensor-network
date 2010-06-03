import java.awt.Color;
import java.awt.GridLayout;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.util.Vector;

import javax.swing.JButton;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.SwingUtilities;

public class SensorNetworkGUI extends JFrame {

	private static final long serialVersionUID = 6364550794187721205L;
	private JPanel cornerPanel;
	private Vector<JPanel> nodes;
	private SerialCommunication serialCom;
	
	private Vector<SensorNode> nodeVector;
	private Vector<SensorNode> oldNodeVector;
	
	private Vector<String> fireVector;
	private Vector<String> oldFireVector;
	
	private JFrame mainFrame;
	private JButton resetButton;
	private ResetButtonListener resetbuttonListener;
	
	/**
	 * Constructor
	 */
	public SensorNetworkGUI(SerialCommunication serialCom){
		this.serialCom = serialCom;
		initComponents();
	}
	
	/**
	 * Destructor
	 */
	public void terminate(){
		mainFrame.dispose();
	}
	
	/**
	 * Initialize components
	 */
	private void initComponents(){			
		GridLayout gridLayout = new GridLayout(4,2,5,5);
		cornerPanel = new JPanel(gridLayout);
		
		nodes = new Vector<JPanel>();
		nodeVector = new Vector<SensorNode>();
		oldNodeVector = new Vector<SensorNode>();
		fireVector = new Vector<String>();
		oldFireVector = new Vector<String>();
		
		mainFrame = new JFrame();
		mainFrame.setDefaultCloseOperation(EXIT_ON_CLOSE);
		mainFrame.setSize(800, 700);
		
		initCornerPanel();
		mainFrame.add(cornerPanel);
		
		checkNewData();
		updateWindow();
	}
	
	/**
	 * Initialize panel that is placed at top left corner
	 */
	private void initCornerPanel(){
		JPanel redPanel = new JPanel();
		redPanel.setBackground(new Color(255,0,0));
		JLabel redLabel = new JLabel("Node on fire");
		cornerPanel.add(redPanel);
		cornerPanel.add(redLabel);
		
		JPanel bluePanel = new JPanel();
		bluePanel.setBackground(new Color(0,0,255));
		JLabel blueLabel = new JLabel("No fire, not exit");
		cornerPanel.add(bluePanel);
		cornerPanel.add(blueLabel);
		
		JPanel greenPanel = new JPanel();
		greenPanel.setBackground(new Color(0,255,0));
		JLabel greenLabel = new JLabel("Exit node");
		cornerPanel.add(greenPanel);
		cornerPanel.add(greenLabel);
		
		resetButton = new JButton("Reset");
		resetbuttonListener = new ResetButtonListener();
		resetButton.addActionListener(resetbuttonListener);
		cornerPanel.add(resetButton);
		
		cornerPanel.setBounds(0, 0, 240, 130);
		cornerPanel.validate();
	}
	
	/**
	 * Check if there is any new data from the network
	 * @return boolean true if there is new data, otherwise false
	 */
	@SuppressWarnings("unchecked")
	public boolean checkNewData(){
		Vector<String> nodeData = serialCom.getData();
		fireVector = serialCom.getFireData();
		boolean newData = false;
		if(nodeData.size()>0){		
			for(int i=0; i<nodeData.size(); i++){
				if(nodeVector.size()<=i){
					nodeVector.addElement(SensorNode.parseSensorNode(nodeData.get(i)));
				}
				else{
					nodeVector.setElementAt(SensorNode.parseSensorNode(nodeData.get(i)),i);
				}				
			}						
			if(!nodeVector.equals(oldNodeVector)){
				oldNodeVector = (Vector<SensorNode>) nodeVector.clone();
				newData = true;
			}
		}
		if(!fireVector.equals(oldFireVector)){
			oldFireVector = (Vector<String>) fireVector.clone();
			
			for(int x=0; x<fireVector.size(); x++){
				String temp = fireVector.get(x);
				boolean fireOccurence = !temp.startsWith("clean:");
				temp = temp.substring(temp.indexOf(':')+1);
				for(int q=0; q<nodeVector.size(); q++){
					if(nodeVector.get(q).getRimeID().equals(temp)){								
						if(fireOccurence){
							SensorNode sn = nodeVector.get(q);
							sn.setOnFire(true);
							nodeVector.setElementAt(sn, q);
							serialCom.changeFireStatus(q,"1");
						}
						else{System.out.println("Node is found");
							SensorNode sn = nodeVector.get(q);
							sn.setOnFire(true);
							nodeVector.setElementAt(sn, q);
							serialCom.changeFireStatus(q,"0");
							serialCom.cleanFire(x);
						}
						oldNodeVector = (Vector<SensorNode>) nodeVector.clone();				
						newData = true;
					}
				}
			}
		}
		return newData;
	}
	
	/**
	 * Updates window so that new data will be visible on screen
	 */
	public void updateWindow(){		
		for(int i=0; i<nodeVector.size(); i++){
			JPanel jPanel = new JPanel();
			JLabel rimeID = new JLabel(nodeVector.get(i).getRimeID());
			int x = nodeVector.get(i).getxCoordinate();
			int y = nodeVector.get(i).getyCoordinate();
			
			jPanel.setBounds(60*x+240, 35*y+130, 50, 25);			
			jPanel.add(rimeID);
			
			if(nodeVector.get(i).isOnFire()){
				jPanel.setBackground(new Color(255,0,0));
			}
			else if(nodeVector.get(i).isExit()){
				jPanel.setBackground(new Color(0,255,0));
			}
			else{
				jPanel.setBackground(new Color(0,0,255));
			}
			
			mainFrame.add(jPanel);
			
			if(nodes.size()<=i){
				nodes.addElement(jPanel);
			}
			else{
				nodes.setElementAt(jPanel, i);
			}
			
			mainFrame.validate();
			mainFrame.repaint();
		}
		mainFrame.add(new JPanel());
		mainFrame.validate();		
		mainFrame.setVisible(true);
	}

	/**
	 * UNUSED!!!
	 * @param args
	 */
	public static void main(String[] args) {		

		SwingUtilities.invokeLater(new Runnable() {
			
			SerialCommunication serialComm = new SerialCommunication("/dev/ttyUSB0","plotMap");
			
			@SuppressWarnings("unused")
			SensorNetworkGUI gui = new SensorNetworkGUI(serialComm);
			
			@Override
			public void run() {
				
			}
		});
	}
	
	/**
	 * When reset button is hit, network is reset and then initialized
	 */
	class ResetButtonListener implements ActionListener {
		public void actionPerformed(ActionEvent e) {
			serialCom.writeSerialData("reset_system_packet");System.out.println("reset_system_packet");
			try {
				Thread.sleep(10000l);
			} catch (InterruptedException ie) {
				System.err.println("Cannot sleep");
			}
			serialCom.writeSerialData("send_init_packet");System.out.println("send_init_packet");
		}	
	}
}
