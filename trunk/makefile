# Suggested call
# CFLAGS=-DPRODUCTION make clean all
# CFLAGS="-g -o2" make clean all

LDFLAGS+= -L. -Lsexpr/src
CPPFLAGS+=-std=c99 -Isexpr/src
.PHONY: clean all test.%
TESTS= draft space program
all: libentrelacs.so


test: $(TESTS:%=test%.o) $(TESTS:%=test%) $(TESTS:%=test.%)

test.% : test%
	-[ -f entrelacs.dat ] && rm entrelacs.dat
	LD_LIBRARY_PATH=. ./$+
	od -t x1z -w8 entrelacs.dat


libentrelacs.so: mem0.o mem.o sha1.o space.o machine.o
	$(LD) $(LDFLAGS) -o $(@) $^ -Lsexpr/src -lsexp -shared -lc

clean:
	-rm -f *.o $(TESTS:%=test%)

*.o: *.h

test%.o: test%.c

testdraft: testdraft.o libentrelacs.so

testspace: testspace.o libentrelacs.so

testprogram: testprogram.o libentrelacs.so
