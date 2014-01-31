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

### Fonts filesystem

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

## items filesystem

    items/
      dir/
        _new01/
          flags
          type ->
        _panel01/
          flags
          type -> grid
          restraint -> Maxwidth Minwidth Maxheight Minheight
          items/
            01/
              path -> buttons/_ok
              place -> Row Rowspan Col Colspan
              sticky -> tblr
              padding ->
            02/
              path ->
              place -> Row Rowspan Col Colspan
              sticky ->
              padding ->
        buttons/
          _ok/
            flags
            type -> button
            restraint -> Maxwidth Minwidth Maxheight Minheight
            text -> Ok
            font -> sans:10:Bold
            g -> X Y W H
          _cancel/
            flags
            type -> button
            restraint -> Maxwidth Minwidth Maxheight Minheight
            text -> Cancel
