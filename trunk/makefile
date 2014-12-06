# Suggested calls:
# CFLAGS=-DPRODUCTION make clean all
# CFLAGS="-g -o0" make clean all
# CFLAGS="-DDEBUG -g -o0" make clean all
# make tests
# make run
# CFLAGS="-g -o0" make clean.testmachine testmachine
# make run.testmachine
# make entrelacsd
# make run.testshell
# make start
# make gdb_start
# make valgrind_start
.PHONY: help server clean all clean.% test% run.% tests run start
CPPFLAGS += -std=c99 -pthread -fPIC

TARGETS = libentrelacs.so libentrelacs.a server
# entrelacsd
OBJECTS = log.o mem0.o mem.o sha1.o space.o machine.o session.o
TESTS = space uri script machine shell

PERSISTENCE_FILE=/tmp/entrelacs_test.dat

all: $(TARGETS)

help:
	@head makefile | grep '^#'

clean: $(TESTS:%=clean.test%) clean.server
	-rm -f $(OBJECTS) $(TARGETS)

clean.server:
	-rm -f mongoose.o server.o entrelacsd

$(TESTS:%=clean.test%):
	-rm $(@:clean.%=%) $(@:clean.%=%.o)

tests: all $(TESTS:%=test%)

test%: test%.o libentrelacs.a
	$(CC) $(LDFLAGS) $^ -o $(@) -lpthread


run: $(TESTS:%=run.%)

run.%: test%
	-[ -f $(PERSISTENCE_FILE) ] && rm $(PERSISTENCE_FILE)
	ENTRELACS=$(PERSISTENCE_FILE) LD_LIBRARY_PATH=. ./$<
	# od -t x1z -w8 $(PERSISTENCE_FILE)

server: entrelacsd

entrelacsd: mongoose.o session.o server.o libentrelacs.a
	$(CC) -B dynamic -pthread -o $(@) $^ -ldl

draft: testdraft.o testdraft run.draft
shell: testshell
prompt: run.shell

libentrelacs.a: $(OBJECTS)
	ar rvs $(@) $^
	
libentrelacs.so: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $(@) $^ -shared -lc

*.o: *.h

start: server
	-pkill entrelacsd
	# -[ -f $(PERSISTENCE_FILE) ] && rm $(PERSISTENCE_FILE)
	ENTRELACS=$(PERSISTENCE_FILE) ./entrelacsd
	od -t x1z -w8 $(PERSISTENCE_FILE)

gdb_start: server
	-pkill entrelacsd
	-[ -f $(PERSISTENCE_FILE) ] && rm $(PERSISTENCE_FILE)
	-[ -f $(PERSISTENCE_FILE).journal ] && rm $(PERSISTENCE_FILE).journal
	CFLAGS="-DDEBUG -g -o0" make clean all	
	ENTRELACS=$(PERSISTENCE_FILE) gdb ./entrelacsd
	od -t x1z -w8 $(PERSISTENCE_FILE)

valgrind_start: server
	-pkill entrelacsd
	-[ -f $(PERSISTENCE_FILE) ] && rm $(PERSISTENCE_FILE)
	-[ -f $(PERSISTENCE_FILE).journal ] && rm $(PERSISTENCE_FILE).journal
	CFLAGS="-DDEBUG -g -o0" make clean all	
	ENTRELACS=$(PERSISTENCE_FILE) valgrind ./entrelacsd
	od -t x1z -w8 $(PERSISTENCE_FILE)
