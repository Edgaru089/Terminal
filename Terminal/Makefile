VPATH = vterm

CPPFLAGS = -DSFML_STATIC
CFLAGS   = -O2
CXXFLAGS = -O2 -Wno-unused-result -std=c++11
LDFLAGS  = -s -Wl,-O2
LDLIBS   = -lsfml-graphics-s -lsfml-window-s -lsfml-system-s -lpthread -lGL -lGLU -lX11 -lXrandr -ludev -lfreetype -lutil -lm -lstdc++

objects       = Main.o Terminal.o Terminal.Draw.o Terminal.Event.o
objects_vterm = encoding.o keyboard.o mouse.o parser.o pen.o screen.o state.o unicode.o vterm.o

all: Terminal

Terminal: $(objects) $(objects_vterm)

clean:
	rm -f *.o Terminal vterm/*.o

.PHONY: all clean

