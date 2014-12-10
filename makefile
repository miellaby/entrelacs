# Suggested calls:
# make run # compile and perform regression tests
# make server # compile the server
# make run.shell # simple REPL
# make start # compile then start the server
# make gdb
# make valgrind
# 
# make run.machine # compile and run one test
# make tests # compile tests
# make clean # clean in order to rebuild
# make clean.testmachine # clean one test
# 
# CFLAGS=-DPRODUCTION make clean all
# CFLAGS="-g -o0" make clean all
# CFLAGS="-DDEBUG -g -o0" make clean all
# CFLAGS="-g -o0" make clean.testmachine testmachine
# 
# make help # this help

.PHONY: help server clean all clean.% test% run.% tests run start
CPPFLAGS += -std=c99 -pthread -fPIC -Wno-format
BINDIR = bin

TARGETS = libentrelacs.so libentrelacs.a entrelacsd
OBJECTS = log.o mem0.o mem.o sha1.o space.o machine.o session.o
OBJECTS_entrelacsd = mongoose.o server.o

TESTS = space uri script machine shell

PERSISTENCE_FILE=/tmp/entrelacs_test.dat


BINTARGETS = $(TARGETS:%=$(BINDIR)/%)
BINOBJECTS = $(OBJECTS:%=$(BINDIR)/%)
BINOBJECTS_entrelacsd = $(OBJECTS_entrelacsd:%=$(BINDIR)/%)

all: $(BINTARGETS)

help:
	@head -30 makefile | grep '^#' | sed -e '/# .*/ s/# \(.*\)/\1/'

clean: $(TESTS:%=clean.test%)
	-rm -f $(BINOBJECTS) $(BINTARGETS) $(BINOBJECTS_entrelacsd)

$(TESTS:%=clean.test%):
	-rm $(BINDIR)/$(@:clean.%=%) $(BINDIR)/$(@:clean.%=%.o)

test.%: $(BINDIR)/test%

tests: all $(TESTS:%=test.%)

server: $(BINDIR)/entrelacsd

draft: $(BINDIR)/testdraft.o $(BINDIR)/testdraft run.draft

shell: $(BINDIR)/testshell

$(BINDIR)/test%: $(BINDIR)/test%.o $(BINDIR)/libentrelacs.a
	$(CC) $(LDFLAGS) $^ -o $(@) -lpthread

$(BINDIR)/libentrelacs.a: $(BINOBJECTS)
	ar rvs $(@) $^
	
$(BINDIR)/libentrelacs.so: $(BINOBJECTS)
	$(LD) $(LDFLAGS) -o $(@) $^ -shared -lc

$(BINDIR)/entrelacsd: $(BINOBJECTS_entrelacsd) $(BINDIR)/session.o $(BINDIR)/libentrelacs.a
	$(CC) -B dynamic -pthread -o $(@) $^ -ldl

$(BINDIR)/*.o: *.h

$(BINDIR)/%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

prompt: run.shell

run: $(TESTS:%=run.%)

run.%: $(BINDIR)/test%
	-[ -f $(PERSISTENCE_FILE) ] && rm $(PERSISTENCE_FILE)
	ENTRELACS=$(PERSISTENCE_FILE) LD_LIBRARY_PATH=. ./$<
	# od -t x1z -w8 $(PERSISTENCE_FILE)

start: server
	-pkill entrelacsd
	# -[ -f $(PERSISTENCE_FILE) ] && rm $(PERSISTENCE_FILE)
	ENTRELACS=$(PERSISTENCE_FILE) $(BINDIR)/entrelacsd
	# od -t x1z -w8 $(PERSISTENCE_FILE)

gdb:
	-pkill entrelacsd
	-[ -f $(PERSISTENCE_FILE) ] && rm $(PERSISTENCE_FILE)
	-[ -f $(PERSISTENCE_FILE).journal ] && rm $(PERSISTENCE_FILE).journal
	CFLAGS="-DDEBUG -g -o0" make clean all	
	ENTRELACS=$(PERSISTENCE_FILE) gdb $(BINDIR)/entrelacsd
	#od -t x1z -w8 $(PERSISTENCE_FILE)

valgrind:
	-pkill entrelacsd
	-[ -f $(PERSISTENCE_FILE) ] && rm $(PERSISTENCE_FILE)
	-[ -f $(PERSISTENCE_FILE).journal ] && rm $(PERSISTENCE_FILE).journal
	CFLAGS="-DDEBUG -g -o0" make clean all	
	ENTRELACS=$(PERSISTENCE_FILE) valgrind $(BINDIR)/entrelacsd
	# od -t x1z -w8 $(PERSISTENCE_FILE)
