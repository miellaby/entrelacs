CPPFLAGS=-std=c99
.PHONY: clean all

all: entrelacs.so

test: testXYZ
	LD_LIBRARY_PATH=. ./testXYZ


entrelacs.so: mem0.o space.o sha1.o entrelacs.o
	$(LD) $(LDFLAGS) -shared -o $(@) mem0.o space.o sha1.o entrelacs.o -lc

clean:
	rm *.o

*.o: *.h

testXYZ: testXYZ.o entrelacs.so