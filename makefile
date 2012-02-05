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

LDFLAGS += -L$(SEXP_LIBPATH)
CPPFLAGS += -std=c99 -I$(SEXP_INCPATH)

TARGETS = libentrelacs.so libentrelacs.a
# entrelacsd
OBJECTS = log.o mem0.o mem.o sha1.o space.o machine.o
TESTS = space fingerprints program machine

PERSISTENCE_FILE=/var/tmp/entrelacs_test.dat

all: $(TARGETS)

help:
	@head makefile | grep '^#'

clean: $(TESTS:%=clean.test%)
	-rm -f $(OBJECTS) $(TARGETS) $(TESTS:%=test%)

$(TESTS:%=clean.test%):
	-rm $(@:clean.%=%) $(@:clean.%=%.o)

tests: all $(TESTS:%=test%.o) $(TESTS:%=test%)

#$(TESTS:%=test%): libentrelacs.so
$(TESTS:%=test%): libentrelacs.a sexpr/src/libsexp.a

run: $(TESTS:%=run.test%)

run.%: %
	-[ -f $(PERSISTENCE_FILE) ] && rm $(PERSISTENCE_FILE)
	ENTRELACS=$(PERSISTENCE_FILE) LD_LIBRARY_PATH=. ./$<
	od -t x1z -w8 $(PERSISTENCE_FILE)

entrelacsd: mongoose.o session.o server.o libentrelacs.a $(SEXP_LIBPATH)/libsexp.a

draft: testdraft.o testdraft run.testdraft
    
libentrelacs.a: $(OBJECTS)
	ar rvs $(@) $^
	
libentrelacs.so: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $(@) $^ -lsexp -shared -lc

*.o: *.h



