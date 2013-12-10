struct file;
struct arr;
struct p9_fid;

void read_data_fn(struct p9_connection *c, int size, char *buf);
void read_str_fn(struct p9_connection *c, int size, char *buf);
void write_data_fn(struct p9_connection *c, int size, char *buf);
void write_buf_fn(struct p9_connection *c, int delta, struct arr **buf);

void read_bool_fn(struct p9_connection *c, int val);
int write_bool_fn(struct p9_connection *c, int oldval);

int file_path_len(struct file *f, struct file *root);
int file_path(int bytes, char *buf, struct file *f, struct file *root);
struct file *find_file(struct file *root, char *path);

void rm_fid_aux(struct p9_fid *fid);
