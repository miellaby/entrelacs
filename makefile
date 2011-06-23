CPPFLAGS=-std=c99
.PHONY: clean all

all: entrelacs.so

test: testXYZ
	rm entrelacs.dat
	LD_LIBRARY_PATH=. ./testXYZ
	od -t x1z -w8 entrelacs.dat



entrelacs.so: mem0.o space.o sha1.o entrelacs.o
	$(LD) $(LDFLAGS) -shared -o $(@) mem0.o space.o sha1.o entrelacs.o -lc

clean:
	rm *.o

*.o: *.h

testXYZ: testXYZ.o entrelacs.so
