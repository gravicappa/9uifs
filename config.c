#include "config.h"
#include "ui.h"

int server_port = 5558;

struct uiobj;

extern int init_uigrid(struct uiobj *u);
extern int init_uiscroll(struct uiobj *u);
extern int init_uilabel(struct uiobj *u);
extern int init_uibutton(struct uiobj *u);
extern int init_uiimage(struct uiobj *u);

struct uiobj_maker uitypes[] = {
  {"grid", init_uigrid},
  {"scroll", init_uiscroll},
  {"label", init_uilabel},
  {"button", init_uibutton},
  {"image", init_uiimage},
  /*
  {"entry", init_uientry},
  {"canvas", init_uicanvas},
  */
  {0, 0}
};
