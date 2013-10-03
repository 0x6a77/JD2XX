include Makefile.conf

all: libftd2xx.a

%: %.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $^ -c -o $@

%.def: $(TARGET)/$(ARCH)/%.dll $(TARGET)/%.h
	$(GENDEF) - $< > $@

lib%.a: $(TARGET)/$(ARCH)/%.dll %.def
	$(DLLTOOL) -A --dllname $*.dll --def $*.def --output-lib $@

lib%.a: $(TARGET)/$(ARCH)/%.dll %.def
	$(DLLTOOL) -A --dllname $*.dll --def $*.def --output-lib $@

clean:
	$(RM) *.def lib*.a
	$(RM) *.o

distclean: clean
	$(RM) *.bak
