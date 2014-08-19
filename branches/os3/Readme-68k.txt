This is a ks3.1 68k port of Hivelytracker v1.8

There are two builds of the main program included. One for 68020+ and
one for 68020+ with fpu.

Recommended configuration for running hivelytracker on 68k

  *Fast CPU (68060 or emulated)
  *Cybergraphx compatible graphics card.
  *Some free ram.
  *AHI (V6 on aminet. However I use the older V4 - http://www.lysator.liu.se/ahi/v4-site/)
  *Some fonts (included)
  *guigfx.library and render.library (included)

This release differs from the OS4 version in the following ways:

  *Includes screenmode requester. To choose another mode use the window/fullscreen
   cycle gadget.

  *Includes configurable mixing frequency (Needs a restart for changes to take effect)

  *Ability to switch off vumeters/wavemeter

  *Additional Skin font size options (Font size restrictions still in place)

  *Uses NewMouse for mousewheel support (Use [ and ] keys to emulate)
  
Installation:

copy the render.library and guigfx.library from libs/ to libs: if needed
install the fonts. Note that the font names have changed since version 1.3.

Run the version applicable to your system

At first start the program will ask you to select a screenmode. It tries to choose the best
mode for the program. Note: No matter what resolution you choose, hively tracker is hard coded
for 800x600, so if you open a small screen, then it will be in autoscroll mode. For graphics card
users I recommend you use an 800x600 resolution in 8 bit. For AGA users, choose 640x512 or similar,
but please make sure you choose a low colour depth. Otherwise you will run out of memory, and
something nasty might happen. I've run the program successfully on AGA in 2 bitplanes.

Have fun

http://www.hivelytracker.co.uk - http://www.exotica.org.uk
