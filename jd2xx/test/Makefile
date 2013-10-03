include ../Makefile.conf

JSRC = $(wildcard *.java)
JOBJ = $(JSRC:%.java=%.class)

JAVAC = $(JDK)/bin/javac

JFLAGS = -O -cp "../jd2xx.jar"

.PRECIOUS: %.class

all:
	$(JAVAC) $(JFLAGS) *.java

%.class: %.java
	$(JAVAC) $(JFLAGS) $^

clean:
	$(RM) $(JOBJ)

distclean: clean
	$(RM) *.bak
