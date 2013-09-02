// package test;

import java.io.IOException;

import jd2xx.JD2XX;

public class TestEEProgram {

	public TestEEProgram() {
	}

	public static void main(String[] args) throws IOException {
		JD2XX jd = new JD2XX();

		jd.open(0);

		JD2XX.ProgramData pd = jd.eeRead();
		System.out.println(pd.toString());


		pd.invertRI = !pd.invertRI;

		jd.eeProgram(pd);

		pd = jd.eeRead();
		System.out.println(pd.toString());

	}
}
