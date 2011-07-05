LDFLAGS+= -L. -Lsexpr/src
CPPFLAGS+=-std=c99 -Isexpr/src
.PHONY: clean all test.%
TESTS= XYZ draft sexp
all: libentrelacs.so


test: $(TESTS:%=test%.o) $(TESTS:%=test%) $(TESTS:%=test.%)

test.% : test%
	-[ -f entrelacs.dat ] && rm entrelacs.dat
	LD_LIBRARY_PATH=. ./$+
	od -t x1z -w8 entrelacs.dat


libentrelacs.so: mem0.o space.o sha1.o entrelacs.o machine.o
	$(LD) $(LDFLAGS) -shared -o $(@) $^ -lc -L sexpr/src -lsexp

clean:
	rm *.o

*.o: *.h

test%.o: test%.c

testdraft: testdraft.o libentrelacs.so

testXYZ: testXYZ.o libentrelacs.so

testsexp: testsexp.o libentrelacs.so sexpr/src/libsexp.a
