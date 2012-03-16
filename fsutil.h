void read_buf_fn(struct p9_connection *c, int size, char *buf);
void write_buf_fn(struct p9_connection *c, int size, char *buf);

void read_bool_fn(struct p9_connection *c, int val);
int write_bool_fn(struct p9_connection *c, int oldval);
