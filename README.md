# lowpix
Tool for programmers to manipulate pixel assets for GBA and such.

## Build

You must generate gl3w code:
    $ cd src-lib/gl3w
    $ python gl3w_gen.py
and glew code:
    $ cd src-lib/glew
    $ make
then generate the projects/makefiles for your platform using premake5.
