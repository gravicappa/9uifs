UI toolkit
==========

This is a relatively simple UI toolkit controlled via 9P.

## Features

  * Controlled via 9P.
  * Provides several controls:
    - text labels,
    - buttons,
    - images,
    - scrollbars,
    - text entries,
    - grid layout manager.
  * Allows to use custom graphic and system frontends. Uses SDL + Imlib2 by
    default.

## Installing

Currently installation requires 9base or p9p since mk and rc are used.
Issuing `mk` command builds the application `uifs`.

## API

Read `docs/doc.md`.

## Samples

See `test/ui` for example. It uses 9pfuse.

Window manager sample `test/wm` uses
[command line 9p-client](https://github.com/gravicappa/9client). It does not
do anything fancy but just switches to last exported control.
