#
# JD2XX Makefile
#

# Override vars in Makefile.conf if needed
-include Makefile.conf

# JAVA_HOME is usually set on Java boxes
JDK ?= $(JAVA_HOME)

PLATFORM=$(shell uname -ms)

# Configuration:
# may be overwritten by environment/commandline variables

CC ?= gcc
OBJDUMP ?= objdump

JFLAGS ?= -O
CFLAGS ?= -O2 -g
LDFLAGS ?=

# OS X
ifeq ($(PLATFORM),Darwin x86_64)
	JDK ?= /Library/Java/JavaVirtualMachines/jdk1.7.0_21.jdk/Contents/Home/
	JDK_HEADERS = $(JDK)/include/darwin
	FTDI = ../ftdi
	OS = osx
	ARCH = static64
	LDFLAGS += -wl -framework CoreFoundation -framework IOKit -lobjc
	SHARED_LIB = libjd2xx.jnilib
#Linux x64
else ifeq ($(PLATFORM),Linux i686)
	JDK ?= /usr/lib/jvm/java-7-oracle/
	JDK_HEADERS = $(JDK)/include/linux
	FTDI = ../ftdi
	OS = linux_x86
	ARCH = i386
	OBJDUMP = objdump
	LDFLAGS += -lrt
	SHARED_LIB = libjd2xx.so
#Linux x86
else ifeq ($(PLATFORM),Linux x86_64)
	JDK ?= /usr/lib/jvm/java-7-oracle/
	JDK_HEADERS = $(JDK)/include/linux
	FTDI = ../ftdi
	OS = linux_x86
	ARCH = x86_64
	CFLAGS += -fPIC
	LDFLAGS += -lrt
	SHARED_LIB = libjd2xx.so
#ARM7
else ifeq ($(PLATFORM),Linux armv7l)
        JDK ?= /usr/lib/jvm/java-7-oracle/
        JDK_HEADERS = $(JDK)/include/linux
        FTDI = ../ftdi
        OS = linux_arm
        CFLAGS += -fPIC
	HARDFLOAT=`readelf -a /usr/bin/readelf | grep armhf` 
	ifeq ($(strip, $(HARDFLOAT)),)
		ARCH="arm926-hf"
	else 
		ARCH="arm926"
	endif 
        LDFLAGS += -lrt
        SHARED_LIB = libjd2xx.so
#Windows (via mingw)
else ifneq ($(findstring MINGW,$(PLATFORM)),)
	ifeq ($(word 2,$(PLATFORM)),i686)
		JDK ?= c:/JDK
		JDK_HEADERS = $(JDK)/include/win32
		FTDI = ../ftdi
		OS = win32
		ARCH = i386
		SHARED_LIB = jd2xx.dll
	else ifeq ($(word 2,$(PLATFORM)),x86_64)
		JDK ?= c:/JDK
		JDK_HEADERS = $(JDK)/include/win32
		FTDI = ../ftdi
		OS = win32
		ARCH = amd64
		SHARED_LIB = jd2xx.dll
	endif
endif

CPPFLAGS += -I$(JDK)/include -I$(JDK_HEADERS) -I$(FTDI) -I$(FTDI)/$(OS) -I./src \
	    -D_JNI_IMPLEMENTATION_

#
# NOTE: We link against the dynamic version of ftd2xx on Windows since its static
# version is compiled with MSVC.
# This is OK since the FTDI driver ships with the DLL.
# Also, generating a separate MinGW import library is unnecessary!
#
LDFLAGS += -shared \
	   -L$(FTDI)/$(OS)/build/$(ARCH)/ -L$(FTDI)/$(OS)/$(ARCH)/ -lftd2xx

JAVAH = $(JDK)/bin/javah -jni
JAVAC = $(JDK)/bin/javac
JAR = $(JDK)/bin/jar
JAVADOC = $(JDK)/bin/javadoc

JSRC = $(wildcard cz/adamh/utils/*.java) \
       $(wildcard jd2xx/*.java)
JOBJ = $(JSRC:%.java=%.class)

#.PRECIOUS: %.class
.PHONY: all clean

all: jd2xx.jar
jni: $(SHARED_LIB)

%.h: jd2xx/JD2XX.class
	$(JAVAH) -classpath . -d src jd2xx.JD2XX

%.lst: %.o
	$(OBJDUMP) -dxStr $< > $@

src/JD2XX.o : src/jd2xx_JD2XX.h src/jd2xx_JD2XX_DeviceInfo.h \
	      src/jd2xx_JD2XX_ProgramData.h

$(SHARED_LIB): src/JD2XX.o
	$(CC) -o $@ $^ $(LDFLAGS)

%.class: %.java
	$(JAVAC) $(JFLAGS) $<

jd2xx.jar: $(JOBJ)
	$(JAR) cf $@ cz/adamh/utils/*.class jd2xx/*.class \
		     jni/

doc:
	$(JAVADOC) -d $@ jd2xx

maven-install: jd2xx.jar pom.xml
	mvn install:install-file -Dfile=jd2xx.jar -DpomFile=pom.xml

clean:
	$(RM) jd2xx.jar $(SHARED_LIB)
	$(RM) jd2xx/*.class cz/adamh/utils/*.class
	$(RM) src/jd2xx_JD2XX*.h src/*.o

distclean: clean
	$(RM) jd2xx/*.bak src/*.bak *.bak
	$(RM) -r doc

printvars:
	@echo $(JOBJ)
	@echo Using JDK: $(JDK)
	@echo JAVA_HOME: $(JAVA_HOME)
	@echo JDK headers: $(JDK_HEADERS)
	@echo Building for OS: $(OS), ARCH: $(ARCH)
