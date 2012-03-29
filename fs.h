struct fid {
  struct p9_fid f;
  struct fid *prev;
  struct fid *next;
  struct fid *fprev;
  struct fid *fnext;
};

struct fid_pool {
  struct fid *fids[256];
  struct fid *free;
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
  struct fid *fids;
  void (*rm)(struct file *);
  union {
    int i;
    void *p;
  } aux;
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

void reset_fids(struct fid_pool *pool);
struct p9_fid *get_fid(unsigned int fid, struct fid_pool *pool);
struct p9_fid *add_fid(unsigned int fid, struct fid_pool *pool, int msize);
void rm_fid(struct p9_fid *fid, struct fid_pool *pool);
void detach_file_fids(struct file *file);
void detach_fid(struct p9_fid *fid);
void attach_fid(struct p9_fid *fid, struct file *file);
void free_fids(struct fid_pool *pool);
