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

extern struct p9_fs fs;
extern unsigned long long qid_cnt;

void add_file(struct file *root, struct file *f);
void rm_file(struct file *f);
struct file *find_file(struct file *root, int size, char *name);

void reset_fids(struct fid_pool *pool);
struct p9_fid *get_fid(unsigned int fid, struct fid_pool *pool);
struct p9_fid *add_fid(unsigned int fid, struct fid_pool *pool, int msize);
void rm_fid(struct p9_fid *fid, struct fid_pool *pool);
void detach_file_fids(struct file *file);
void detach_fid(struct p9_fid *fid);
void attach_fid(struct p9_fid *fid, struct file *file);
void free_fids(struct fid_pool *pool);

void resp_file_create(struct p9_connection *c, struct file *f);

#define DEFFILE(f, n, m, a) do { \
    (f).name = (n); \
    (f).mode = (m); \
    (f).qpath = ++qid_cnt; \
    (f).aux.p = (a); \
  } while (0)
