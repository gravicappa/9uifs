typedef void *UImage;
typedef void *UFont;

extern int screen_w;
extern int screen_h;
extern UImage screen_image;

void set_cliprect(int x, int y, int w, int h);
void fill_rect(UImage dst, int x, int y, int w, int h, unsigned int c);
void draw_rect(UImage dst, int x, int y, int w, int h, unsigned int fg,
               unsigned int bg);
void draw_line(UImage dst, int x1, int y1, int x2, int y2, unsigned int c);
void draw_poly(UImage dst, int npts, int *pts, unsigned int fg,
               unsigned int bg);

UImage create_image(int w, int h, void *rgba);
void free_image(UImage img);
void image_write_rgba(UImage img, unsigned int off_bytes, int len_bytes,
                      void *rgba);
void image_read_rgba(UImage img, unsigned int off_bytes, int len_bytes,
                    void *rgba);
int image_get_size(UImage img, int *w, int *h);
UImage resize_image(UImage img, int w, int h, int flags);
void blit_image(UImage dst, int dx, int dy, int dw, int dh,
                UImage src, int sx, int sy, int sw, int sh);

void draw_utf8(UImage dst, int x, int y, int c, UFont fnt, int len, char *s);
int get_utf8_size(UFont font, int len, char *str, int *w, int *h);

UFont create_font(const char *name, int size, const char *style);
void free_font(UFont font);
const char **font_list(int *n);

unsigned int current_time_ms(void);
