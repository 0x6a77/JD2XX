JD2XX
=====

A JD2XX fork that supports OS X (x86_64) and Linux (x86, x86_64, arm9hf, arm9sf)

JD2XX allows Java programs to control any FTDI serial UART bridge IC that has D2XX support.

FTDI UART bridges are good to control external devices: robots, dataloggers, sniffers, legacy equipment, etc.

My focus is OS X and Linux, especially ARM-based single-board computers (SBC) like ODroid, Raspberry Pi and BeagleBoard.

The ARM Linux variants support hard and soft float. First figure out your ARM Linux flavor (hard or soft) and then get the right Java version for it. As of 10MAY13 Java 7 ARM is soft float only and Java 8 ARM Preview is hard float only. With the included Makefile you should automagically get an ARM JNI lib with the correct float flavor simply by typing "make". If you're clever you'll figure out how to set the java.library.path system property (-D on the command line is one way) to find the right native library flavor for your Java program. Most JVMs now require these native libraries to load with an absolute path - keep that in mind.