# Suggested call
# CFLAGS=-DPRODUCTION make clean all
# CFLAGS="-g -o2" make clean all
.PHONY: clean all test.%

LDFLAGS+= -L. -Lsexpr/src
CPPFLAGS+=-std=c99 -Isexpr/src
TESTS= space program machine

all: libentrelacs.so libentrelacs.a

clean:
	-rm -f *.o *.so $(TESTS:%=test%)

draft: testdraft.o testdraft test.draft

tests: $(TESTS:%=test%.o) $(TESTS:%=test%)

run: $(TESTS:%=run.test%)

run.% : %
	-[ -f entrelacs.dat ] && rm entrelacs.dat
	LD_LIBRARY_PATH=. ./$<
	od -t x1z -w8 entrelacs.dat
    
libentrelacs.a: mem0.o mem.o sha1.o space.o machine.o
	ar rvs $(@) $^
	
libentrelacs.so: mem0.o mem.o sha1.o space.o machine.o
	$(LD) $(LDFLAGS) -o $(@) $^ -lsexp -shared -lc

*.o: *.h

#$(TESTS:%=test%): libentrelacs.so
$(TESTS:%=test%): libentrelacs.a sexpr/src/libsexp.a



