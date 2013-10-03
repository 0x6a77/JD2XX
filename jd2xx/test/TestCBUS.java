// package test;

import java.io.IOException;

import jd2xx.JD2XX;

public class TestCBUS extends Thread {

	public TestCBUS() {
	}

	public static void main(String[] args) throws IOException, InterruptedException {
		JD2XX jd = new JD2XX();

		jd.open(0);

		/* Testing CBUS0,1 as output - will Flash LP1,2 in EVAL232R board in BBIO */
		jd.setBitMode(0x30 | 0x01, 0x20); // enable CBUS bit banging
		System.out.println(Integer.toHexString(jd.getBitMode()));

		for (int i=0; i<10; ++i) {
			int v = jd.getBitMode();
			System.out.println(Integer.toHexString(v));
			sleep(250);
			jd.setBitMode(0x10 | (v ^ 0x3), 0x20);
		}
		sleep(250);

		jd.setBitMode(0, 0);
	}
}
