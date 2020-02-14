
CPPFLAGS =
CXXFLAGS = -g -O2 -Wno-unused-result -std=c++11
LDFLAGS  = -pthread -s -Wl,-O2
LDLIBS   = -lsfml-network -lsfml-system -lutil -lm -lstdc++

objects = WslBackend.o

all: wsl-backend

wsl-backend: $(objects)
	cc -o wsl-backend $(objects) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f *.o wsl-backend

.PHONY: all clean

