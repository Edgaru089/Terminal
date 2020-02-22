# Terminal
A terminal emulator based on [libvterm](http://www.leonerd.org.uk/code/libvterm/) and [SFML](https://github.com/SFML/SFML).

## Why I made this
 - [ConEmu](https://github.com/Maximus5/ConEmu) which I was using experience all sorts of issue when displaying too much CJK characters, coloring output, and even using vim.
 - And I really hate introducing a complete copy of Chromium, parsing static HTML, interpreting code written in a dynamic-type scripting language and suffering performance issues on an old PC I'm using (that's what [Hyper](https://github.com/zeit/hyper) and other terminal emulators based on Electron is doing).

So I made this in like 5 days.

## What it supports

It does support:
 - Everything libvterm supports (coloring, scrollback, altscreen, underline, mouse, setting title, most XTerm features)
 - Cross-platform (works both on Windows and Linux)
 - CJK (combining Hangul does not work)
 - Calling ioctl() on resize in WSL (with built-in WslFrontend and bundled wsl-backend)

It does not, but might in the future, support:
 - Selecting text
 - Custom color palette (now it uses a libvterm default, which looks like XTerm colors)
 - Using multiple fonts (so that you won't need to craft your own combined font)

It will not (in the near future) support:
 - Everything libvterm does not support (80/132 column modes, double-sized text, etc.)
 - RTL Formatting
 - Arabic, combining Hangul and other combining characters having more than 1 codepoint per cell (SFML poorly supports that)

## How to build:

Prerequisites: SFML (Tested with 2.5.1) and a C++11-compliant compiler (Tested with VC++14.2(2019) and g++-7). libvterm is bundled.

### Windows with Visual Studio:

Install x64 SFML under C:\Non-System\Libraries64 (the default).
Be sure to build a static version and check SFML_USE_STATIC_STD_LIBS to link with static VC++.

Open Terminal.sln and compile the x64 target. By default it links with static VC++ and static SFML.

#### To use WSL

Be sure to build a x64 version. (wsl.exe is under Windows/System32 under x64 Windows)

Under WSL Shell, chdir to Terminal/wsl-backend and type "make".

### Linux:

Install SFML graphics, window and system modules. You'll also need the development packages of:

```
GL GLU X11 Xrandr udev freetype
```

Enter the folder "Terminal" and type "make". This links with static SFML.
