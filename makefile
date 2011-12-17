# Suggested calls:
# CFLAGS=-DPRODUCTION make clean all
# CFLAGS="-g -o2" make clean all
# CFLAGS="-DPRODUCTION -DDEBUG_MACHINE -g -o2" make clean all
# make tests
# make run
# CFLAGS="-g -o2" make clean.testmachine testmachine
# make run.testmachine

.PHONY: clean all clean.% test.% run.% help

SEXP_LIBPATH = sexpr/src
SEXP_INCPATH = sexpr/src

LUUID_LIBPATH = /usr/lib
LUUID_INCPATH = /usr/include

LDFLAGS += -L$(SEXP_LIBPATH) -L$(LUUID_LIBPATH)
CPPFLAGS += -std=c99 -I$(SEXP_INCPATH) -I$(LUUID_INCPATH)

TARGETS = libentrelacs.so libentrelacs.a
OBJECTS = log.o mem0.o mem.o sha1.o space.o machine.o
TESTS = space fingerprints program machine

PERSISTENCE_DIR=~/.entrelacs/

all: $(TARGETS)

help:
	@head makefile | grep '^#'

clean: $(TESTS:%=clean.test%)
	-rm -f $(OBJECTS) $(TARGETS) $(TESTS:%=test%)

$(TESTS:%=clean.test%):
	-rm $(@:clean.%=%) $(@:clean.%=%.o)

tests: all $(TESTS:%=test%.o) $(TESTS:%=test%)

#$(TESTS:%=test%): libentrelacs.so
$(TESTS:%=test%): libentrelacs.a sexpr/src/libsexp.a /usr/lib/libuuid.a

run: $(TESTS:%=run.test%)

run.% : %
	-[ -f $(PERSISTENCE_DIR)entrelacs.dat ] && rm $(PERSISTENCE_DIR)entrelacs.dat
	LD_LIBRARY_PATH=. ./$<
	od -t x1z -w8 $(PERSISTENCE_DIR)entrelacs.dat

draft: testdraft.o testdraft run.testdraft
    
libentrelacs.a: $(OBJECTS)
	ar rvs $(@) $^
	
libentrelacs.so: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $(@) $^ -lsexp -shared -lc

*.o: *.h



