#define P9_VERSION "9P2000"
#define P9_NOTAG ((ushort)~0)
#define P9_NOFID (~0u)

enum {
  P9_TVersion = 100,
  P9_RVersion,
  P9_TAuth = 102,
  P9_RAuth,
  P9_TAttach = 104,
  P9_RAttach,
  P9_TError = 106,  /* illegal */
  P9_RError,
  P9_TFlush = 108,
  P9_RFlush,
  P9_TWalk = 110,
  P9_RWalk,
  P9_TOpen = 112,
  P9_ROpen,
  P9_TCreate = 114,
  P9_RCreate,
  P9_TRead = 116,
  P9_RRead,
  P9_TWrite = 118,
  P9_RWrite,
  P9_TClunk = 120,
  P9_RClunk,
  P9_TRemove = 122,
  P9_RRemove,
  P9_TStat = 124,
  P9_RStat,
  P9_TWStat = 126,
  P9_RWStat
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
enum { P9_QTDIR = 0x80, /* type bit for directories */
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

  unsigned int data_len;
  char *data;

  struct p9_stat stat;

  struct p9_qid qid;
  struct p9_qid aqid;

  struct p9_qid wqid[P9_MAXWELEM];
};

int p9_pack_stat(int bytes, unsigned char *buf, struct p9_stat *stat);
int p9_unpack_stat(int bytes, unsigned char *buf, struct p9_stat *stat);

struct p9_connection {
  int fd;
  int msize;
  char *inbuf;
  char *outbuf;
  p9_msg t;
  p9_msg r;
};
