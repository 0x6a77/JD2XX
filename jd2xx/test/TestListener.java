

import java.io.IOException;

import jd2xx.JD2XX;
import jd2xx.JD2XXEvent;
import jd2xx.JD2XXEventListener;

public class TestListener implements Runnable, JD2XXEventListener {

	public TestListener() {
	}

	public static void main(String[] args) throws Exception {
		TestListener tl = new TestListener();

		JD2XX jd = new JD2XX();
		jd.open(0);

		jd.addEventListener(tl);
		jd.notifyOnEvent(JD2XX.EVENT_RXCHAR | JD2XX.EVENT_MODEM_STATUS, true);

		Thread tester = new Thread(tl);
		tester.start();

		try { Thread.sleep(15*1000); }
		catch (InterruptedException e) {
			System.out.println("InterruptedException");
		}

		jd.notifyOnEvent(~0, false);
	}

	public void jd2xxEvent(JD2XXEvent ev) {
		JD2XX jd = (JD2XX)ev.getSource();
		int et = ev.getEventType();

		try {
			if ((et & JD2XX.EVENT_RXCHAR) != 0) {
				int r = jd.getQueueStatus();
				System.out.println("RX event: " + new String(jd.read(r)));
			}
			if ((et & JD2XX.EVENT_MODEM_STATUS) != 0) {
				System.out.println("Modem status event");
			}
		}
		catch (IOException e) {
			System.out.println("IOException");
		}
	}

	public void run() {
		try {
			while (true) {
				Thread.sleep(1*1000);
				System.out.println("Too much work and no play makes Duke a dull boy");
			}
		}
		catch (InterruptedException e) {
			System.out.println("InterruptedException");
		}
	}

}
