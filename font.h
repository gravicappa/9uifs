#define FONT_B "b"
#define FONT_I "i"
#define FONT_BI "bi"

UFont font_from_str(const char *str);
int init_fonts();
void free_fonts();

int init_fonts_fs(struct file *fs);
