struct ctl_cmd {
  char *name;
  void (*fn)(char *cmd, void *aux);
};

struct file *mk_ctl(char *name, struct ctl_cmd *cmd, void *aux);
