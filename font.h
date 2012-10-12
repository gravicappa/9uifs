#define FONT_B "b"
#define FONT_I "i"
#define FONT_BI "bi"

const char *font_file(const char *name, const char *style);
int init_fonts();
void free_fonts();

int init_fonts_fs(struct file *fs);
