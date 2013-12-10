# Application FS structure

    root/
      images/
        <picname>/
          ctl
          rgba
          size
          in.png
      fonts/
        list
        ...
      ui/
      bus/
      store/ ; local fs

# Images filesystem

      images/
        pic0/
          ctl
          rgba
          size
          in.png
        pic1/
          ctl
          rgba
          size
          in.png
        ...

# Bus filesystem

      bus/
        sys/
          ev/
            out
          kbd/
            out
          pointer/
            out
          wm/
            in
        user1/
          in
          out
        ...

## Standard events

    :ptr in Widget
    :ptr out Widget
    :ptr u Id X Y Btn Widget
    :ptr d Id X Y Btn Widget
    :ptr m Id X Y Dx Dy Btns Widget
    :key u Key State Unicode Widget
    :key d Key State Unicode Widget
    :resize Rx Ry Rw Rh Widget

    :press_button Widget

    :exported App-id Widget
    :unexported App-id Widget

*to be defined*

### Pointer events

Move pointer

    ptr m Id X Y Dx Dy Btn-bitmask Widget

* _Id_: pointer index (for multitouch interfaces)
* _X_, _Y_: pointer coordinates
* _Dx_, _Dy_: delta from previous coordinates
* _Btn-bitmask_: bit-mask of button press
* _Widget_: event's widget

> Maybe _Btn-bitmask_ should be a list of pressed buttons

Press pointer

    ptr d Id X Y Btn Widget

* _Id_: pointer index (for multitouch interfaces)
* _X_, _Y_: pointer coordinates
* _Btn_: number of pressed button
* _Widget_: event's widget

Release pointer

    ptr u Id X Y Btn

* _Id_: pointer index (for multitouch interfaces)
* _X_, _Y_: pointer coordinates
* _Btn_: number of released button
* _Widget_: event's widget
  
### Keyboard events

    key d Keysym Mod-bitmask Unicode Widget
    key u Keysym Mod-bitmask Unicode Widget

* _Keysym_: keysym
* _Mod-bitmask_: state of modifier keys
* _Unicode_: unicode number of pressed character, or -1 if not applicable
* _Widget_: event's widget

## Commands

    set_desktop App-id Widget

## Image

    /
      ctl
      rgba
      size
      in.png

- *ctl*:
- *size*: contains string `width height` which defines size in pixels.
- *rgba*: contains `width × height × bytes-per-pixel` bytes of RGBA pixel
          data.
- *in.png*: file for loading `png` files.

### image/ctl

*Commands:*

    blit "Img" [Src-x Src-y Src-w Src-h Dst-x Dst-y Dst-w Dst-h]
    rect RGBA_outline RGBA_fill X1 Y1 W1 H1 [... Xn Yn Wn Hn]
    line RGBA_outline Xs1 Ys1 Xe1 Ye1 [... Xsn Ysn Xen Yen]
    text Font RGBA_fill X Y Text
    poly RGBA_outline RGBA_fill X1 Y1 ... Xn Yn
    ...

### gl

*to be defined*

### images

    /
      imagedir1/
        _image1
        _image2
        imagedir2/
          _image3
      imagedir3/
        _image4
      _image5
      ...

*to be defined*

### fonts

    /
      list

*to be defined*

## ui filesystem

      /
        dir/
          _new01/
            flags
            type ->
          _panel01/
            flags
            type -> grid
            restraint -> Maxwidth Minwidth Maxheight Minheight
            container
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
              container
            _cancel/
              flags
              type -> button
              restraint -> Maxwidth Minwidth Maxheight Minheight
              text -> Cancel
              container

