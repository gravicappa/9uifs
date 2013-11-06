struct ctl_cmd {
  char *name;
  void (*fn)(struct file *f, char *cmd);
};

struct file *mk_ctl(char *name, struct ctl_cmd *cmd);
