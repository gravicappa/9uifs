#define FONT_B "b"
#define FONT_I "i"
#define FONT_BI "bi"

UFont font_from_str(const char *str);
int init_fonts(void);
void free_fonts(void);

struct file *mk_fonts_fs(const char *name);
