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

Then, to draw pixels, on the standard input you can use the commands: `#rrggbb` to use the given color,`@p` to move the cursor to position `p`, and `*l` to draw `l` pixels in the current color.  For example, sending the line:

    #ff0000@500*100

would draw red starting at pixel 500 for 100 pixels.

## `pixbard`

`pixbard` is a simple daemon that will create a pixbar with a fixed number of named segments which can be changed to a fixed number of named colors, as set up in `$HOME/.pixbard`.  An example configuration file will be created if one does not yet exist.

## `pixbarc`

`pixbarc` communicates with `pixbard` to change the status shown in the various segments of the pixbar.

Pass `--help` to get the usage message:

    % pixbarc --help
    usage: pixbarc [name [status]]

For example, to make the "email" segment turn the "active" color, one would just do:

    pixbarc email active

If no color name or no color name and segment name is given on the command line, then those values can be taken continuously on the standard input.

## Credit

Most information about using XCB came from two sources:

- [XCB tutorial](http://xcb.freedesktop.org/tutorial/)
- [bar ain't recursive - A lightweight xcb based bar](https://github.com/LemonBoy/bar)
