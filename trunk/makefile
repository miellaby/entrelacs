# Suggested calls:
# CFLAGS=-DPRODUCTION make clean all
# CFLAGS="-g -o2" make clean all
# CFLAGS="-DPRODUCTION -DDEBUG_MACHINE -g o2" make clean all
# make tests
# make run
# CFLAGS="-g -o2" make clean.testmachine testmachine
# make run.testmachine

.PHONY: clean all clean.% test.% run.% help

LDFLAGS+=-L. -Lsexpr/src
CPPFLAGS+=-std=c99 -Isexpr/src
TARGETS=libentrelacs.so libentrelacs.a
OBJECTS=mem0.o mem.o sha1.o space.o machine.o
TESTS=space program machine

all: $(TARGETS)

help:
	@head makefile | grep '^#'

clean: $(TESTS:%=clean.test%)
	-rm -f $(OBJECTS) $(TARGETS) $(TESTS:%=test%)

$(TESTS:%=clean.test%):
	-rm $(@:clean.%=%) $(@:clean.%=%.o)

tests: $(TESTS:%=test%.o) $(TESTS:%=test%)

#$(TESTS:%=test%): libentrelacs.so
$(TESTS:%=test%): libentrelacs.a sexpr/src/libsexp.a

run: $(TESTS:%=run.test%)

run.% : %
	-[ -f entrelacs.dat ] && rm entrelacs.dat
	LD_LIBRARY_PATH=. ./$<
	od -t x1z -w8 entrelacs.dat

draft: testdraft.o testdraft run.testdraft
    
libentrelacs.a: $(OBJECTS)
	ar rvs $(@) $^
	
libentrelacs.so: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $(@) $^ -lsexp -shared -lc

*.o: *.h



