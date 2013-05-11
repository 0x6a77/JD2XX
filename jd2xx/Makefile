#
# JD2XX Makefile
#


platform=$(shell uname -ms)

ifeq ($(platform),Darwin x86_64)
	JDK = /Library/Java/JavaVirtualMachines/jdk1.7.0_21.jdk/Contents/Home/
	FTDI = ../ftdi
	OS = osx
	ARCH = static64

	CC = /usr/bin/gcc
	OBJDUMP = /usr/bin/objdump
	SHARED_LIB = libjd2xx.jnilib
else

endif

JSRC = $(wildcard jd2xx/JD2XX*.java)
JOBJ = $(JSRC:%.java=%.class)

TARGETS = jd2xx/JD2XX.class src/jd2xx_JD2XX.h JD2XX.jar $(SHARED_LIB)

INC = -I$(JDK)/include -I$(JDK)/include/darwin -I$(FTDI) -I$(FTDI)/$(OS) -I./src
DEF = -D_JNI_IMPLEMENTATION_
LIB = -L$(FTDI)/$(OS)/build/$(ARCH)/ -lftd2xx -framework CoreFoundation -framework IOKit -lobjc

JAVAH = $(JDK)/bin/javah -jni
JAVAC = $(JDK)/bin/javac
JAR = $(JDK)/bin/jar
JAVADOC = $(JDK)/bin/javadoc

JFLAGS = -O
CFLAGS = -O2 -g $(INC) $(DEF)
LDFLAGS = -shared -Wl

vpath %.c src
vpath %.h src

.PRECIOUS: %.class
.PHONY: all clean
.INTERMEDIATE: JD2XX.o

all: $(TARGETS)

%.class: $(JSRC)
	$(JAVAC) $(JFLAGS) $^

%.h:
	$(JAVAH) -classpath . -d src $(notdir $(subst _,.,$*))

%.lst: %.o
	$(OBJDUMP) -dxStr $< > $@

%.o: %c %h
	$(CC) -c $(CFLAGS) $< -o $@

$(SHARED_LIB): JD2XX.o
	$(CC) $(LDFLAGS) -o $(notdir $@) $^ $(LIB) 

%.jar: $(JOBJ)
	$(JAR) cf $@ jd2xx/*

doc:
	$(JAVADOC) -d $@ jd2xx

clean:
	$(RM) $(TARGETS) jd2xx/JD2XX*.class src/jd2xx_JD2XX*.h JD2XX.o

distclean: clean
	$(RM) jd2xx/*.bak src/*.bak *.bak
	$(RM) -r doc
