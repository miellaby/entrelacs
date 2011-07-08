# Suggested call
# CFLAGS=-DPRODUCTION make clean all
# CFLAGS="-g -o2" make clean all
.PHONY: clean all test.%


LDFLAGS+= -L. -Lsexpr/src
CPPFLAGS+=-std=c99 -Isexpr/src
TESTS= draft space program machine

all: libentrelacs.so

clean:
	-rm -f *.o *.so test%.out


test: $(TESTS:%=test%.o) $(TESTS:%=test%) $(TESTS:%=test.%)

test.% : test%
	-[ -f entrelacs.dat ] && rm entrelacs.dat
	LD_LIBRARY_PATH=. ./$<
	od -t x1z -w8 entrelacs.dat
    

libentrelacs.so: mem0.o mem.o sha1.o space.o machine.o
	$(LD) $(LDFLAGS) -o $(@) $^ -Lsexpr/src -lsexp -shared -lc


*.o: *.h

$(TESTS:%=test%): libentrelacs.so


