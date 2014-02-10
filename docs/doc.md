.Table-of-contents

# General

And application creates a tree of controls then marks those which with their
descendants it want to show to user as `exported`. If a «window manager»
application exists it receives notifications about exported controls and shows
them on screen according to its internal logic. See `set_wm` description to
learn more about window manager applications.

# Application FS structure

    /
      images/
      fonts/
      items/
      bus/

# Images filesystem

It represents images application can operate. Images can be created from png
files or can be created from scratch. You can manipulate them on pixel level
or using drawing commands.

    images/
      dir0/
        _pic0/
          ctl
          rgba
          size
          in.png
        _pic1/
          ctl
          rgba
          size
          in.png
        ...

Images can be organized in directories. The distinction among images and
directories is currently stupid: when created directory name starts with `_`
(underscore) it represents an image, otherwise it is just a directory.

## Image filesystem

    Image/
      ctl
      id
      rgba
      size
      in.png

- *ctl*: for applying drawing commands on this image.
- *id*: unique ID of image. Used in ctl-commands.
- *size*: contains string `Width Height` which defines size in pixels. Writing
          new values resizes the image.
- *rgba*: contains `Width × Height × Bytes-per-pixel` bytes of RGBA pixel
          data. It is writable.
- *in.png*: for loading PNG images. Write PNG-encoded image to this file and 
            the image will be loaded.

### Commands of `images/Image/ctl` 

#### blit
  
    blit Img_id [Src-x Src-y Src-w Src-h Dst-x Dst-y Dst-w Dst-h]

Draws other image addressed by its `id`. If `Src-w != Dst-w` or 
`Dst-h != Dst-h` then image is scaled when drawn.

#### rect

    rect RGBA_outline RGBA_fill X1 Y1 W1 H1 [... Xn Yn Wn Hn]

Draws a rectangle or a series of rectangles.

#### line

    line RGBA_outline Xs1 Ys1 Xe1 Ye1 [... Xsn Ysn Xen Yen]

Draws a line or a set of lines.

#### text

    text Font RGBA_fill X Y Text

Draws a text with given font.

#### poly

    poly RGBA_outline RGBA_fill X1 Y1 ... Xn Yn

Draws a polyline.

# Fonts filesystem

Currently it can only list fonts available for application.

    fonts/
      list

`list` file contains a list of available fonts.

# Bus filesystem

This facility is used for getting events and basic communication with system.

    bus/
      sys
      kbd
      pointer
      ev

## Standard events on `bus/ev`

### ptr

    ptr in Id Control
    ptr out Id Control
    ptr d Id X Y Btn Control
    ptr u Id X Y Btn Control
    ptr m Id X Y Dx Dy Btns Control

* `in`: pointer `Id` enters `Control`
* `out`: pointer `Id` leaves `Control`
* `d`: button `Btn` of pointer `Id` is pressed on `Control`
* `u`: button `Btn` of pointer `Id` is released on `Control`
* `m`: pointer `Id` is moved by `Dx`, `Dy` to `X`, `Y` over `Control` with
       buttons `Btns` pressed.

### key

    key d Key State Unicode Control
    key u Key State Unicode Control

* `d`: key `Key` is pressed over `Control` with unicode symbol `Unicode` and
       modifiers `State`.
* `u`: key `Key` is released over `Control` with unicode symbol `Unicode` and
       modifiers `State`.

### resize

    resize Rx Ry Rw Rh Control

`Control` is resized to `{Rx, Ry, Rw, Rh}` rectangle.

### press_button

    press_button Control

`Control` of type "button" is pressed.

### (un)exported

    exported App-id/Control
    unexported App-id/Control

Application `App-id` exported or unexported `Control`. Exported controls can
be addressed by other applications and by window manager application.

## Commands on `bus/sys`

### set_wm

    set_wm Enabled

If `Enabled` is not `0` then application issued this command is promoted to
window manager else it is demoted from window manager. Application must be
local.

### set_desktop

    set_desktop App-id/Control

`Control` of application `App-id` is set to "root" window of UI. Only window
manager application can call this command.

# items filesystem

    items/
      _new01/
      _panel01/
      buttons/
        _ok/
        _cancel/
        side_buttons/
          _help/

Controls (as images) can be organized in arbitrary tree. The distinction among
controls and directories is stupid: when created directory name starts with
`_` (underscore) it represents a control, otherwise it is a directory.

## Generic control filesystem

After creation control has no type, its filesystem looks like this

    Control/
      type
      background
      g
      restraint
      flags

Filesystem extended after an appropriate type is written to `type` file.
Currently supported types are:

  - `grid` — a layout management control,
  - `scroll` — a control that allows scrolling of contained control,
  - `label` — a simple text control,
  - `button` — a button control,
  - `image` — an image displaying control,
  - `entry` — a text entry control.

Once set type cannot be changed.

### background

A background colour of a control. Supported color formats are (each letter
denotes hex character):

    - RRGGBBAA — 0xAARRGGBB
    - RRGGBB — 0xffRRGGBB
    - RGBA — 0xAfRfGfBf
    - RGB — 0xffRfGfBf
    - AA — 0xAAffffff
    - A — 0xAfffffff

### g

Readonly file representing geometry of a control in form `X Y Width Height`.

### restraint

File limiting the size of a control in form `Maxwidth Maxheight`.

### flags

File contains a list of control's properties. Supported delimiters are space,
newline, tab. Properties are:

  - `ev_kbd` — control emits keyboard events to `/bus/ev`.
  - `ev_ptr_move` — control emits pointer move events to `/bus/ev`,
  - `ev_ptr_updown` — control emits pointer press events to `/bus/ev`,
  - `ev_ptr_intersect` — control emits pointer enter/leave events to
                         `/bus/ev`,
  - `ev_resize` — control emits resize events to `/bus/ev`,
  - `exported` — control is accessible to other applications and to window
                 manager.

## Grid control

This is a layout manager similar to Tk's grid(n).

    Grid_control/
      type
      background
      g
      restraint
      flags
      colsopts
      rowsopts
      items/

### colsopts,  rowsopts

Flags for columns and rows. Each file contains a list of integers. Each list
item represents flag of a corresponding column or row. If a value is `1` then
corresponding column or row size is not limited to size of controls it
contains.

### items/
    
    Grid_control/items/_some_item
      sticky
      padding
      place
      path

#### sticky

Sets how embedded control is placed in a grid cell if the latter is larger. It
can contain any combination of four letters 'n', 's', 'e', 'w' that define to
which edges of a cell the control sticks.

#### padding

Contains a list of four integers that represent padding space around embedded
control. The order is left, top, right, bottom.

#### place

Contains a list of four integers that sets the coordinates of an embedded
control in a grid. The format is `Column Row Column-span Row-span`.
`Column-span` and `Row-span` are set to 1 if not defined.

#### path

A relative to root path to control to embed. If control is exported by another
application then the path starts with application's ID.

## Scroll control

    Scroll_control/
      type
      background
      g
      restraint
      flags
      scrollpos
      expand
      items/
        0/

### scrollpos

A pair of integers describing the position of embedded control.

### expand

A pair of integers that defines whether scroll control size expands to
corresponding size of embedded control.

### items/0

A predefined placeholder for embedding a control.

## Label control

    Label_control/
      type
      background
      g
      restraint
      flags
      text
      foreground
      font

### text

The text of a label. It can contain newlines.

### foreground

The text colour. Format is the same as for `background` file.

### font

The text font. It should be a string in format `Font:size`.

## Button control

    Button_control/
      type
      background
      g
      restraint
      flags
      text
      foreground
      font

Its files are similar to Label's.

## Image control

    Control/
      type
      background
      g
      restraint
      flags
      path

### path
Contains a path to embedded image.

## Entry control

    Entry_control/
      type
      background
      g
      restraint
      flags
      text
      foreground
      font
      caret

### caret
Contains a caret's position.
