#define P9_VERSION "9P2000"
#define P9_NOTAG ((unsigned short)~0)
#define P9_NOFID (~0u)

enum {
  P9_XSTART = 100,
  P9_TVERSION = 100,
  P9_RVERSION,
  P9_TAUTH = 102,
  P9_RAUTH,
  P9_TATTACH = 104,
  P9_RATTACH,
  P9_TERROR = 106,  /* Illegal */
  P9_RERROR,
  P9_TFLUSH = 108,
  P9_RFLUSH,
  P9_TWALK = 110,
  P9_RWALK,
  P9_TOPEN = 112,
  P9_ROPEN,
  P9_TCREATE = 114,
  P9_RCREATE,
  P9_TREAD = 116,
  P9_RREAD,
  P9_TWRITE = 118,
  P9_RWRITE,
  P9_TCLUNK = 120,
  P9_RCLUNK,
  P9_TREMOVE = 122,
  P9_RREMOVE,
  P9_TSTAT = 124,
  P9_RSTAT,
  P9_TWSTAT = 126,
  P9_RWSTAT,
  P9_XEND
};

enum {
  P9_OREAD = 0, /* open for read */
  P9_OWRITE = 1,  /* write */
  P9_ORDWR = 2, /* read and write */
  P9_OEXEC = 3, /* execute, == read but check execute permission */
  P9_OTRUNC = 16, /* or'ed in (except for exec), truncate file first */
  P9_OCEXEC = 32, /* or'ed in, close on exec */
  P9_ORCLOSE = 64,  /* or'ed in, remove on close */
  P9_ODIRECT = 128, /* or'ed in, direct access */
  P9_ONONBLOCK = 256, /* or'ed in, non-blocking call */
  P9_OEXCL = 0x1000,  /* or'ed in, exclusive use (create only) */
  P9_OLOCK = 0x2000,  /* or'ed in, lock after opening */
  P9_OAPPEND = 0x4000 /* or'ed in, append only */
};

/* bits in Qid.type */
enum {
  P9_QTDIR = 0x80, /* type bit for directories */
  P9_QTAPPEND = 0x40, /* type bit for append only files */
  P9_QTEXCL = 0x20, /* type bit for exclusive use files */
  P9_QTMOUNT = 0x10,  /* type bit for mounted channel */
  P9_QTAUTH = 0x08, /* type bit for authentication file */
  P9_QTTMP = 0x04,  /* type bit for non-backed-up file */
  P9_QTSYMLINK = 0x02,  /* type bit for symbolic link */
  P9_QTFILE = 0x00  /* type bits for plain file */
};

/* bits in Dir.mode */
enum {
  P9_DMEXEC = 0x1,  /* mode bit for execute permission */
  P9_DMWRITE = 0x2, /* mode bit for write permission */
  P9_DMREAD = 0x4  /* mode bit for read permission */
};

#define P9_DMDIR  0x80000000  /* mode bit for directories */
#define P9_DMAPPEND 0x40000000  /* mode bit for append only files */
#define P9_DMEXCL 0x20000000  /* mode bit for exclusive use files */
#define P9_DMMOUNT  0x10000000  /* mode bit for mounted channel */
#define P9_DMAUTH 0x08000000  /* mode bit for authentication file */
#define P9_DMTMP  0x04000000  /* mode bit for non-backed-up file */

struct p9_qid {
  unsigned char type;
  unsigned int version;
  unsigned long long path;
};

#define P9_MAXWELEM 16

struct p9_stat {
  unsigned short size;
  unsigned short type;
  unsigned int dev;
  struct p9_qid qid;
  unsigned int mode;
  unsigned int atime;
  unsigned int mtime;
  unsigned long long length;
  unsigned int name_len;
  char *name;
  unsigned int uid_len;
  char *uid;
  unsigned int gid_len;
  char *gid;
  unsigned int muid_len;
  char *muid;
};

struct p9_msg {
  unsigned int size;
  unsigned int type;
  unsigned short tag;
  unsigned short oldtag;
  unsigned int msize;
  unsigned int iounit;
  unsigned int afid;
  unsigned int fid;
  unsigned int mode;
  unsigned int perm;
  unsigned int newfid;
  unsigned int nwname;
  unsigned int nwqid;
  unsigned int count;
  unsigned long long offset;

  unsigned int wname_len[P9_MAXWELEM];
  char *wname[P9_MAXWELEM];

  unsigned int version_len;
  char *version;
  unsigned int uname_len;
  char *uname;
  unsigned int aname_len;
  char *aname;
  unsigned int ename_len;
  char *ename;
  unsigned int name_len;
  char *name;

  char *data;

  struct p9_stat stat;

  struct p9_qid qid;
  struct p9_qid aqid;

  struct p9_qid wqid[P9_MAXWELEM];
  int deferred;
  struct p9_fid *pfid;
};

#define P9_WRITE_MODE(mode) \
  ((((mode) & 3) == P9_OWRITE) || (((mode) & 3) == P9_ORDWR))
#define P9_READ_MODE(mode) \
  ((((mode) & 3) == P9_OREAD) || (((mode) & 3) == P9_ORDWR))

#define P9_SET_STR(f, str) do { \
    f = (str); \
    f ## _len = strlen((str)); \
  } while (0)

int p9_stat_size(struct p9_stat *stat);
int p9_pack_stat(int bytes, char *buf, struct p9_stat *stat);
int p9_unpack_stat(int bytes, char *buf, struct p9_stat *stat);
int p9_unpack_msg(int bytes, char *buf, struct p9_msg *m);
int p9_pack_msg(int bytes, char *buf, struct p9_msg *m);

struct p9_fid {
  unsigned int fid;
  struct p9_qid qid;
  char open_mode;
  char owns_uid;
  char *uid;
  unsigned int iounit;
  void (*rm)(struct p9_fid *);
  void *file;
  void *aux;
};

struct p9_connection {
  int msize;
  struct p9_msg t;
  struct p9_msg r;
  void *buf;
  void *flushed;
  void *aux;
};

struct p9_fs {
  void (*version)(struct p9_connection *c);
  void (*auth)(struct p9_connection *c);
  void (*attach)(struct p9_connection *c);
  void (*flush)(struct p9_connection *c);
  void (*walk)(struct p9_connection *c);
  void (*walk1)(struct p9_connection *c, struct p9_fs *fs);
  void (*open)(struct p9_connection *c);
  void (*create)(struct p9_connection *c);
  void (*read)(struct p9_connection *c);
  void (*write)(struct p9_connection *c);
  void (*clunk)(struct p9_connection *c);
  void (*remove)(struct p9_connection *c);
  void (*stat)(struct p9_connection *c);
  void (*wstat)(struct p9_connection *c);
};

int p9_process_treq(struct p9_connection *c, struct p9_fs *fs);
