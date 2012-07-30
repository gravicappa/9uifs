void read_data_fn(struct p9_connection *c, int size, char *buf);
void write_data_fn(struct p9_connection *c, int size, char *buf);
void write_buf_fn(struct p9_connection *c, int delta, struct arr **buf);

void read_bool_fn(struct p9_connection *c, int val);
int write_bool_fn(struct p9_connection *c, int oldval);

int file_path(struct arr **buf, struct file *f, struct file *root);
struct file *find_file_path(struct file *root, int size, char *path);
