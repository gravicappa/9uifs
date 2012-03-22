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
        0/
        1/
        2/
        ...
      store/
      comm/

## event

*to be defined*

## view
    /
      event
      pointer -> [<idx> <x> <y>]
      kbd -> [<key1> <key2> ...]
      joystick -> []
      geometry
      blit/
      gl/
      canvas/
      ui/

### view/event

*to be defined*

### view/pointer

Move pointer

    m <i> <x> <y> <btn-bitmask>

Press pointer

    d <i> <x> <y> <btn>

Release pointer

    u <i> <x> <y> <btn>
  
### view/kbd

    d <keysym> <mod-bitmask> <unicode>
    u <keysym> <mod-bitmask> <unicode>

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

    blit img x y
    blit img srcx srcy srcw srch dstx dsty dstw dsth

### gl

*to be defined*

### canvas

    /
      ctl
      tags/
        tag0/
          ctl
          bbox
          objs
        ...
        tagN/
      objs/
        obj0/
          ctl
          type
          bbox
          data
        ...
        objM/

### images

*to be defined*

### fonts

*to be defined*

### ui

      ui/
        uievent
        type -> panel
        g -> x y w h
        placement -> []
        visible -> 0 | 1
        w/
          panel01/
            evfilter
            type -> panel
            g -> x y w h
            visible -> 0 | 1
            placement -> x: 0 y: 0 w: 2 h: 1 sticky: nsew
            w/
              text01/
                evfilter
                type -> label
                g -> x y w h
                text -> "Blahblah"
                visible -> 0 | 1
          ok/
            evfilter
            type -> button
            g -> x y w h
            text -> Ok
            font -> sans:10:Bold
            placement -> x: 0 y: 1 w: 1 h: 1 sticky: lrtb
            visible -> 0 | 1
            image/
              pic -> pic0
                     2 0 2 0
              placement -> left
          cancel/
            uievent
            type -> button
            g -> x y w h
            text -> Cancel
            visible -> 0 | 1
            placement -> x: 1 y: 1 w: 1 h: 1 sticky: nsew

#### ui/uievent

*to be defined*

#### ui/type

*to be defined*

#### ui/placement

*to be defined*

#### ui/visible

*to be defined*
