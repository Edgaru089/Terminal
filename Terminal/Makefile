VPATH = vterm imgui

CPPFLAGS =
CFLAGS   = -O2
CXXFLAGS = -O2 -Wno-unused-result -std=c++17 -I/usr/include/freetype2
LDFLAGS  = -Wl,-O2 -s
LDLIBS   = -lsfml-graphics -lsfml-window -lsfml-system -lpthread -lGL -lGLU -lX11 -lXrandr -ludev -lfreetype -lutil -lm -lstdc++fs -lstdc++

objects       = Main.o Terminal.o Terminal.Draw.o Terminal.Event.o gui.o\
				SystemFrontend.o SystemFrontend.Windows.o SystemFrontend.Unix.o
objects_vterm = encoding.o keyboard.o mouse.o parser.o pen.o screen.o state.o unicode.o vterm.o
objects_imgui = imgui.o imgui_demo.o imgui_draw.o imgui_freetype.o imgui_widgets.o imgui-SFML.o

all: Terminal

Terminal: $(objects) $(objects_vterm) $(objects_imgui)

clean:
	rm -f *.o Terminal vterm/*.o

.PHONY: all clean

