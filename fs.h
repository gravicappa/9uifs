struct file_info {
  int owns_name;
  char *name;
  unsigned int mode;
  unsigned int version;
  unsigned long long qpath;
  unsigned long size;
  struct p9_fs *fs;
};

struct file {
  struct file *parent;
  struct file *next;
  struct file *child;
  int owns_name;
  char *name;
  unsigned int mode;
  unsigned int version;
  unsigned long long qpath;
  unsigned long length;
  struct p9_fs *fs;
  void (*rm)(struct file *);
  union {
    int i;
    void *p;
  } context;
};

struct fs_entry {
  char *name;
  unsigned char qtype;
  int type;
  unsigned char perm;
  unsigned char flags;
  void (*set)(struct file *file);
};

extern struct p9_fs fs;
extern unsigned long long qid_cnt;

void add_file(struct file *f, struct file *root);
void rm_file(struct file *f);
