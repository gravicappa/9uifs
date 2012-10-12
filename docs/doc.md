# Application FS structure

    root/
      event -> [? create destroy]
      views/
        <viewname-1>/
        <viewname-2>/
      images/
        <picname>/
          format
          data
          size
      fonts/
        ctl
        sans/
        sans-serif/
        monospace/
        decorative/
        ...
      ui/
      store/
      comm/

## event

*to be defined*

## view
    /
      event
      pointer
      keyboard
      joystick
      g
      visible
      blit/
      gles/
      uiplace/
        padding
        path -> dir/_panel01
        sticky -> 'tblr'
      uisel -> dir/_button01
      canvas/

### view/event

*to be defined*

### view/pointer

Move pointer

    m P X Y Btn-bitmask
    m P X Y Dx Dy Btn-bitmask (?)

* _P_: pointer index (for multitouch interfaces)
* _X_, _Y_: pointer coordinates
* _Dx_, _Dy_: delta from previous coordinates (?)
* _Btn-bitmask_: bit-mask of button press

> Maybe _Btn-bitmask_ should be a list of pressed buttons
> Maybe _Dx_, _Dy_ is useful

Press pointer

    d P X Y Btn

* _P_: pointer index (for multitouch interfaces)
* _X_, _Y_: pointer coordinates
* _Btn_: number of pressed button

Release pointer

    u P X Y Btn

* _P_: pointer index (for multitouch interfaces)
* _X_, _Y_: pointer coordinates
* _Btn_: number of released button
  
### view/kbd

    d Keysym Mod-bitmask Unicode
    u Keysym Mod-bitmask Unicode

### view/geometry

*to be defined*

## blit

    /
      ctl
      pixels
      size
      format

- *ctl*:
- *size*: contains string `width height` which defines size in pixels.
- *pixels*: contains `width × height × bytes-per-pixel` bytes of pixel data.
- *format*: `RGBA8`, probably optional

### blit/ctl

*Commands:*

    blit Img X Y
    blit Img Srcx Srcy Srcw Srch Dstx Dsty Dstw Dsth

### gl

*to be defined*

### canvas

*to be defined*

### images

*to be defined*

### fonts

*to be defined*

## ui

      /
        dir/
          _new01/
            evfilter
            type ->
          _panel01/
            evfilter
            type -> grid
            visible -> 0 | 1
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
              evfilter
              type -> button
              visible -> 0 | 1
              restraint -> Maxwidth Minwidth Maxheight Minheight
              text -> Ok
              font -> sans:10:Bold
              g -> X Y W H
              container
            _cancel/
              evfilter
              type -> button
              visible -> 0 | 1
              restraint -> Maxwidth Minwidth Maxheight Minheight
              text -> Cancel
              container

### ui/uievent

*to be defined*

### ui/type

*to be defined*

### ui/placement

*to be defined*

### ui/visible

*to be defined*
