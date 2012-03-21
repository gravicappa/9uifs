struct ctl_cmd {
  char *name;
  void (*fn)(struct file *f, char *cmd);
};

struct ctl_context {
  struct buf buf;
};

struct ctl_file {
  struct file file;
  struct ctl_cmd *cmd;
};

extern struct p9_fs ctl_fs;
