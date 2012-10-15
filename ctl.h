struct ctl_cmd {
  char *name;
  void (*fn)(struct file *f, char *cmd);
};

struct ctl_context {
  struct arr *buf;
};

struct ctl_file {
  struct file f;
  struct ctl_cmd *cmd;
};

extern struct p9_fs ctl_fs;
