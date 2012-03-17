## WM FS structure

    root/
      event
      windows/
        0/
          ctl
          title
          appname
          g -> x y w h
          fullscreen
          managed
          pixels
        1/
        ...
        N/

## Application FS structure

    root/
      event -> [keyboard mouse joystick ... create destroy]
      views/
        0/
          event
          geometry
          blit/
          gl/
          canvas/
          ui/
      images/
        ctl
        pic0/
          format
          data
          size
      fonts/
        ctl
        sans/
        sans-serif/
        monotype/
        ...
      store/
      comm/

### event

### blit

    /
      ctl
      pixels
      size
      format

- *ctl*:
- *size*: contains string `width height` which defines size in pixels.
- *pixels*: contains `width × height × bytes-per-pixel` bytes of pixel data.
- *format*: `RGBA8`, probably optional

### gl
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
### fonts
### ui

      ui/
        uievent
        type -> panel
        placement -> []
        visible -> true
        w/
          panel01/
            uievent
            type -> panel
            visible -> true
            placement -> x: 0 y: 0 w: 2 h: 1 sticky: nsew
            w/
              text01/
                type -> label
                text -> "Blahblah"
                visible -> true
                uievent
          ok/
            uievent
            type -> button
            text -> Ok
            font -> sans:10:Bold
            placement -> x: 0 y: 1 w: 1 h: 1 sticky: lrtb
            visible -> true
            image/
              pic -> pic0
                     2 0 2 0
              placement -> left
          cancel/
            uievent
            type -> button
            text -> Cancel
            visible -> true
            placement -> x: 1 y: 1 w: 1 h: 1 sticky: nsew

#### ui/uievent
#### ui/type
#### ui/placement
#### ui/visible
