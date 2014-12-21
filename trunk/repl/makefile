# Suggested calls:
# make all
# 
# CFLAGS=-DPRODUCTION make clean all
# CFLAGS="-DDEBUG -g -o0" make clean all
# 
# make help # this help

.PHONY:
CPPFLAGS += -std=c99 -pthread -fPIC -Wno-format -I..
BINDIR = bin

TARGETS = entrelacs
OBJECTS = repl.o linenoise.o

TESTS = 

PERSISTENCE_FILE=/tmp/entrelacs_test.dat

BINTARGETS = $(TARGETS:%=$(BINDIR)/%)
BINOBJECTS = $(OBJECTS:%=$(BINDIR)/%)

all: $(BINTARGETS)

help:
	@head -30 makefile | grep '^#' | sed -e '/# .*/ s/# \(.*\)/\1/'

clean: $(TESTS:%=clean.test%)
	-rm -f $(BINOBJECTS) $(BINTARGETS)

$(TESTS:%=clean.test%):
	-rm $(BINDIR)/$(@:clean.%=%) $(BINDIR)/$(@:clean.%=%.o)

test.%: $(BINDIR)/test%

tests: all $(TESTS:%=test.%)

$(BINDIR)/entrelacs: $(BINOBJECTS) ../bin/libentrelacs.a
	$(CC) $(LDFLAGS) -o $(@) $^ -lpthread

$(BINDIR)/*.o: *.h

$(BINDIR)/%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run: $(TESTS:%=run.%)

run.%: $(BINDIR)/test%
	-[ -f $(PERSISTENCE_FILE) ] && rm $(PERSISTENCE_FILE)
	ENTRELACS=$(PERSISTENCE_FILE) LD_LIBRARY_PATH=. ./$<
	# od -t x1z -w8 $(PERSISTENCE_FILE)
