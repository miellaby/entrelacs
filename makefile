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

.PHONY: help server clean all clean.% test.% run.% tests run start
CPPFLAGS +=
CFLAGS += -std=c99 -pthread -fPIC -I. -I$(CURDIR) -I$(CURDIR)/sha1 -Wall -Wextra
BINDIR = bin

TARGETS = libentrelacs.so libentrelacs.a entrelacsd
VPATH := log:sha1:mem:space:machine:server:test
OBJECTS = log.o mem0.o geoalloc.o mem.o mem_log.o sha1.o \
  hash.o cell.o assimilate.o serial.o weaver.o space.o \
  context.o transient.o machine.o session.o url.o uri.o
OBJECTS_entrelacsd = mongoose.o server.o

TESTS = space uri script machine shell

UTESTS = hash

TEST_EXECUTABLES = $(TESTS:%=$(BINDIR)/test%) $(UTESTS:%=$(BINDIR)/utest%)

.SECONDARY: $(TEST_EXECUTABLES)


PERSISTENCE_FILE=/tmp/entrelacs_test.dat


BINTARGETS = $(TARGETS:%=$(BINDIR)/%)
BINOBJECTS = $(OBJECTS:%=$(BINDIR)/%)
BINOBJECTS_entrelacsd = $(OBJECTS_entrelacsd:%=$(BINDIR)/%)

all: $(BINTARGETS)

tests: $(TEST_EXECUTABLES)

help:
	@head -30 makefile | grep '^#' | sed -e '/# .*/ s/# \(.*\)/\1/'

clean:
	-rm -f $(BINOBJECTS) $(BINTARGETS) $(BINOBJECTS_entrelacsd) $(TEST_EXECUTABLES)

server: $(BINDIR)/entrelacsd

draft: $(BINDIR)/testdraft.o $(BINDIR)/testdraft run.draft

shell: $(BINDIR)/testshell

$(BINDIR)/utesthash: $(BINDIR)/log.o $(BINDIR)/sha1.o
$(BINDIR)/utest%: $(BINDIR)/utest_%.o $(BINDIR)/%.o $(UTEST_%_OBJS)
	$(CC) $(LDFLAGS) $^ -o $(@) -lpthread

$(BINDIR)/test%: $(BINDIR)/test%.o $(BINDIR)/libentrelacs.a
	$(CC) $(LDFLAGS) $^ -o $(@) -lpthread

$(BINDIR)/libentrelacs.a: $(BINOBJECTS)
	ar rvs $(@) $^

$(BINDIR)/libentrelacs.so: $(BINOBJECTS)
	$(LD) $(LDFLAGS) -o $(@) $^ -shared -lc

$(BINDIR)/entrelacsd: $(BINOBJECTS_entrelacsd) $(BINDIR)/session.o $(BINDIR)/libentrelacs.a
	$(CC) -B dynamic -pthread -o $(@) $^ -ldl

$(BINDIR)/%.o: %.h

$(BINDIR)/%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BINDIR)/%.o: test/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

prompt: run.shell

run: $(TESTS:%=run.test%) $(UTESTS:%=run.utest%)

run.%: $(BINDIR)/%
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
	CFLAGS="-DDEBUG -g -o0" make clean all $(BINDIR)/testmachine
	ENTRELACS=$(PERSISTENCE_FILE) gdb $(BINDIR)/testmachine
	# $(BINDIR)/entrelacsd
	#od -t x1z -w8 $(PERSISTENCE_FILE)

valgrind:
	-pkill entrelacsd
	-[ -f $(PERSISTENCE_FILE) ] && rm $(PERSISTENCE_FILE)
	-[ -f $(PERSISTENCE_FILE).journal ] && rm $(PERSISTENCE_FILE).journal
	CFLAGS="-DDEBUG -g -o0" make clean all $(BINDIR)/testmachine
	ENTRELACS=$(PERSISTENCE_FILE) valgrind --leak-check=full $(BINDIR)/testmachine
	# od -t x1z -w8 $(PERSISTENCE_FILE)
