# Z80Explorer
Visual Zilog Z80 netlist-level simulator

Z80 Explorer is a Zilog Z80 netlist-level simulator capable of running Z80 machine code and an educational tool with features that help reverse engineer and understand this chip better.

Application's User's Guide: [Users Guide](https://gdevic.github.io/Z80Explorer)
<br>
It is also described in a blog: https://baltazarstudios.com/z80explorer
<br>
A brief overview is on YouTube: https://youtu.be/_dyngzTEnvw
<br>
The annotated overview is on Vimeo: https://vimeo.com/439468449

## Before you run it

Read the User's Guide.<br>

![Z80 Explorer](https://baltazarstudios.com/wp-content/uploads/2020/07/z80explorer-app.png)

## Getting a pre-built binary (Windows)

A pre-built binary for Windows can be found in the release section of github here:<br>
https://github.com/gdevic/Z80Explorer/releases

## Compiling from sources

The application is built on the Qt framework.

* On Windows, install MS Visual Studio 2022 Community Edition. On Linux, use gcc
* Install Qt 6.9.3 framework (with support for x64 MS Visual Studio 2022 on Windows)
* Compile with QtCreator selecting the “Release” build

On Linux, follow the guide here: https://doc.qt.io/qt-6/linux.html
* sudo apt-get install build-essential libgl1-mesa-dev
* sudo apt-get install libxcb-xinerama0
* Download for Open Source users
* Check to include "Desktop gcc 64-bit"
* Compile with QtCreator selecting the “Release” build

## Compiling zmac assember on Linux

If you are going to compile and run Z80 test programs, you need zmac assembler. Download it from here: http://48k.ca/zmac.html<br>
I am not aware of a prebuilt binary for Linux, but it is fairly easy to build since the source is available at that site. You may also need:
* sudo apt-get install bison
