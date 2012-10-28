# Application FS structure

    root/
      event -> [? create destroy]
      views/
        <viewname-1>/
        <viewname-2>/
      images/
        <picname>/
          ctl
          rgba
          size
          png
      fonts/
        list
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

    m P X Y Dx Dy Btn-bitmask

* _P_: pointer index (for multitouch interfaces)
* _X_, _Y_: pointer coordinates
* _Dx_, _Dy_: delta from previous coordinates
* _Btn-bitmask_: bit-mask of button press

> Maybe _Btn-bitmask_ should be a list of pressed buttons

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
      rgba
      size
      png

- *ctl*:
- *size*: contains string `width height` which defines size in pixels.
- *rgba*: contains `width × height × bytes-per-pixel` bytes of RGBA pixel
          data.

### blit/ctl

*Commands:*

    blit "Img" [Src-x Src-y Src-w Src-h Dst-x Dst-y Dst-w Dst-h]
    rect RGBA_outline RGBA_fill X1 Y1 W1 H1 [... Xn Yn Wn Hn]
    line RGBA_outline Xs1 Ys1 Xe1 Ye1 [... Xsn Ysn Xen Yen]
    text Font RGBA_fill X Y Text
    poly RGBA_outline RGBA_fill X1 Y1 ... Xn Yn
    ...

### gl

*to be defined*

### canvas

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
