CFLAGS += -std=c1x -Isrc -Isrc/vendor -Isrc/vendor/crc-32c -Wall -msse4.2
CFLAGS += -g -O0 #debug
# CFLAGS += -Os

OS := $(shell uname -s)
MACHINE := $(shell uname -m)
include Makefile.$(OS)

BINARYNAME = hfsinspect
BUILDDIR = build/$(OS)-$(MACHINE)
BINARYPATH = $(BUILDDIR)/$(BINARYNAME)
OBJDIR = $(BUILDDIR)/obj

AUXFILES := Makefile Makefile.Darwin Makefile.Linux README.md
PROJDIRS := src/hfsinspect src/hfs src/hfsplus src/logging src/vendor/memdmp src/vendor/crc-32c/crc32c src/volumes
SRCFILES := $(shell find $(PROJDIRS) -type f -name *.c)
HDRFILES := $(shell find $(PROJDIRS) -type f -name *.h)
OBJFILES := $(patsubst src/%.c, $(OBJDIR)/%.o, $(SRCFILES))
DEPFILES := $(patsubst src/%.c, $(OBJDIR)/%.d, $(SRCFILES))
ALLFILES := $(SRCFILES) $(HDRFILES) $(AUXFILES)

INSTALL = $(shell which install)
PREFIX = /usr/local

.PHONY: all test docs clean distclean pretty

all: depend $(BINARYPATH)
everything: $(BINARYPATH) docs
clean: clean-hfsinspect
distclean: clean-test clean-docs
	$(RM) -r build

pretty:
	find src -iname '*.[hc]' -and \! -path '*vendor*' -and \! -path '*Apple*' | uncrustify -c uncrustify.cfg -F- --replace --no-backup --mtime -lC

depend: .depend

.depend: $(SRCFILES)
	@echo "Building dependancy graph"
	@rm -f ./.depend
	@$(CC) $(CFLAGS) -MM $^>>./.depend;

-include .depend

$(OBJDIR)/%.o : src/%.c
	@echo Compiling $<
	@mkdir -p `dirname $@`
	@$(COMPILE.c) $< -o $@

$(BINARYPATH): $(OBJFILES)
	@echo "Building hfsinspect."
	@mkdir -p `dirname $(BINARYPATH)`
	@$(LINK.c) -o $(BINARYPATH) $^ $(LIBS)

docs:
	@echo "Building documentation."
	@doxygen docs/doxygen.config

install: $(BINARYPATH)
	@echo "Installing hfsinspect in $(PREFIX)"
	@mkdir -p $(PREFIX)/bin
	@$(INSTALL) $(BINARYPATH) $(PREFIX)/bin
	@echo "Installing manpage in $(PREFIX)"
	@mkdir -p $(PREFIX)/share/man/man1
	@$(INSTALL) docs/hfsinspect.1 $(PREFIX)/share/man/man1

uninstall:
	$(RM) $(PREFIX)/bin/$(BINARYNAME)
	$(RM) $(PREFIX)/share/man/man1/hfsinspect.1

test: all
	gunzip < images/test.img.gz > images/test.img
	./tools/tests.sh $(BINARYPATH) images/test.img

clean-test:
	$(RM) "images/test.img" "images/MBR.dmg"

clean-hfsinspect:
	$(RM) -r "$(BUILDDIR)" ./.depend

clean-docs:
	$(RM) -r docs/html docs/doxygen.log

dist.tgz: $(ALLFILES)
	tar -czf dist.tgz $^
