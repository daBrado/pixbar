# pixbar

`pixbar` makes a thin X window that is anchored to one side of the screen, whose pixel colors can be addressed and changed via the standard input.

The idea is that it can be used for a very minimal status display.

To that end, also included is a simple Ruby script `pixbard` that provides a daemon for such a status bar concept, and the script `pixbarc` which allows communication with the daemon.

## Install

Run `make` to create the `pixbar` executable, and `make install` to put it and the other scripts in place (you may want to adjust `PREFIX`), e.g.

    make && make install PREFIX=$HOME/opt/pixbar

## `pixbar`

You can get the usage message by running it without arguments:

    % pixbar
    usage: pixbar <x> <y> <width> <length> <edge>

`x` and `y` is the position of the bar on the screen, assumed to be anchored to one edge.

`width` and `length` are independent of orientation; `width` is the thickness of the bar in pixels, and `length` is how many pixels are in the bar.

`edge` is which edge the bar is anchored to.  This can be `l`, `r`, `t`, or `b`, for left, right, top, or bottom, respectively.

For example, to create a 1-pixel-wide pixbar that is anchored to the entire bottom of a 1080p display:

    pixbar 0 1080 1 1920 b

Then, to draw pixels, on the standard input one sends a binary string, 3 bytes per pixel representing red, green, and blue, for the total length of the bar.

## `pixbard`

`pixbard` is a simple daemon that will create a pixbar with a fixed number of named segments which can be changed to a fixed number of named colors, as set up in `$HOME/.pixbard`.  An example configuration file exists here as `pixbard.conf`.

## `pixbarc`

`pixbarc` communicates with `pixbard` to change the status shown in the various segments of the pixbar.

Pass `--help` to get the usage message:

    % pixbarc --help
    usage: pixbarc [name [status]]

For example, to make the "email" segment go to the "personal" state, one would just do:

    pixbarc email personal

If no state name or no state name and segment name is given on the command line, then those values can be taken continuously on the standard input.

## Credit

Most information about using XCB came from two sources:

- [XCB tutorial](http://xcb.freedesktop.org/tutorial/)
- [bar ain't recursive - A lightweight xcb based bar](https://github.com/LemonBoy/bar)
- [XCB programming is hard](http://vincentsanders.blogspot.com/2010/04/xcb-programming-is-hard.html)
