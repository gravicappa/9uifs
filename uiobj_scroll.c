static void
rm_uiscroll(struct file *f)
{
  ;
  ui_rm_uiobj(f);
}

int
init_uiscroll(struct uiobj *u)
{
  struct uiobj_scroll *us;
  u->data = malloc(sizeof(struct uiobj_scroll));
  if (!u->data)
    return -1;
  memset(u->data, 0, sizeof(struct uiobj_scroll));

  u->flags |= UI_IS_CONTAINER;
  u->fs.rm = rm_uigrid;

  return 0;
}
