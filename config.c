#include "config.h"

#include "9p.h"
#include "fs.h"
#include "prop.h"
#include "uiobj.h"

extern int init_uigrid(struct uiobj *u);
extern int init_uiscroll(struct uiobj *u);
extern int init_uilabel(struct uiobj *u);
extern int init_uibutton(struct uiobj *u);

struct uiobj_maker uitypes[] = {
  {"grid", init_uigrid},
  {"scroll", init_uiscroll},
  {"label", init_uilabel},
  {"button", init_uibutton},
  /*
  {"entry", init_uientry},
  {"blit", init_uientry},
  */
  {0, 0}
};
