#
# JD2XX Makefile
#

# Override vars in Makefile.conf if needed
-include Makefile.conf

# JAVA_HOME is usually set on Java boxes
JDK ?= $(JAVA_HOME)

PLATFORM=$(shell uname -ms)

ifeq ($(PLATFORM),Darwin x86_64)
	JDK ?= /Library/Java/JavaVirtualMachines/jdk1.7.0_21.jdk/Contents/Home/
	JDK_HEADERS = $(JDK)/include/darwin
	FTDI = ../ftdi
	OS = osx
	ARCH = static64
	CC = /usr/bin/gcc
	OBJDUMP = /usr/bin/objdump
	LIB_EXTRAS = -wl -framework CoreFoundation -framework IOKit -lobjc
	SHARED_LIB = libjd2xx.jnilib
else ifeq ($(PLATFORM),Linux i686)
	JDK ?= /usr/lib/jvm/java-7-oracle/
	JDK_HEADERS = $(JDK)/include/linux
	FTDI = ../ftdi
	OS = linux_x86
	ARCH = x86
	CC = /usr/bin/gcc
	OBJDUMP = /usr/bin/objdump
	LIB_EXTRAS = -lrt
	SHARED_LIB = libjd2xx.so
else ifeq ($(PLATFORM),Linux x86_64)
	JDK ?= /usr/lib/jvm/java-7-oracle/
	JDK_HEADERS = $(JDK)/include/linux
	FTDI = ../ftdi
	OS = linux_x86
	ARCH = x86_64
	CC = /usr/bin/gcc
	OBJDUMP = /usr/bin/objdump
	CFLAGS = -fPIC
	LIB_EXTRAS = -lrt
	SHARED_LIB = libjd2xx.so
endif

JSRC = $(wildcard jd2xx/JD2XX*.java)
JOBJ = $(JSRC:%.java=%.class)

TARGETS = jd2xx/JD2XX.class src/jd2xx_JD2XX.h jd2xx.jar $(SHARED_LIB)

INC = -I$(JDK)/include -I$(JDK_HEADERS) -I$(FTDI) -I$(FTDI)/$(OS) -I./src
DEF = -D_JNI_IMPLEMENTATION_
LIB = -L$(FTDI)/$(OS)/build/$(ARCH)/ -lftd2xx $(LIB_EXTRAS)

JAVAH = $(JDK)/bin/javah -jni
JAVAC = $(JDK)/bin/javac
JAR = $(JDK)/bin/jar
JAVADOC = $(JDK)/bin/javadoc

JFLAGS = -O
CFLAGS += -O2 -g $(INC) $(DEF)
LDFLAGS = -shared

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

printvars:
	@echo Using JDK: $(JDK)
	@echo JAVA_HOME: $(JAVA_HOME)
	@echo JDK headers: $(JDK_HEADERS)
	@echo Building for OS: $(OS), ARCH: $(ARCH)
