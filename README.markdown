![screenshot](http://github.com/acdimalev/acdimalev-demo-2010-05-02-03/raw/master/screenshot.png)

A C+SDL+Cairo demo.  Fly around and destroy asteroids.  Multiplayer with Xbox 360 gamepads.

http://github.com/acdimalev/acdimalev-demo-2010-05-02-03

You must have Cairo and SDL installed to build this demo.

* http://cairographics.org
* http://www.libsdl.org

This demo was developed on a Debian Lenny system with the following dev packages installed:

* libcairo2-dev=1.6.4-7
* libsdl1.2-dev=1.2.13-2

To build and execute the demo, run "make" and then "./demo".

    $ make
    $ ./demo

The demo will pop up in a window, and may be closed with the *q* key.  You can then use the following controls to manipulate the triangle:

* Keyboard
** Left &mdash; steer left
** Right &mdash; steer right
** Shift &mdash; gas
** Control &mdash; spawn / shoot
* Xbox 360 Controller
** Left Thumbstick &mdash; steer
** Right Trigger &mdash; gas
** A &mdash; spawn / shoot

