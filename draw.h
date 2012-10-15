#define RGBA_A(c) (((c) >> 24) & 0xff)
#define RGBA_R(c) (((c) >> 16) & 0xff)
#define RGBA_G(c) (((c) >> 8) & 0xff)
#define RGBA_B(c) ((c) & 0xff)

#define RGBA(r, g, b, a) (((unsigned int)(a) << 24) \
                          | ((unsigned int)(r) << 16) \
                          | ((unsigned int)(g) << 8) \
                          | ((unsigned int)(b)))

#define RGBA_READ_HEX(x) ((x >= '0' && x <= '9') \
                          ? x - '0' \
                          : (x >= 'a' && x <= 'f') ? x - 'a' + 10 : 0)

#define RGBA_FROM_STR(s) \
  RGBA((RGBA_READ_HEX(s[2]) << 4) | (RGBA_READ_HEX(s[3])), \
       (RGBA_READ_HEX(s[4]) << 4) | (RGBA_READ_HEX(s[5])), \
       (RGBA_READ_HEX(s[6]) << 4) | (RGBA_READ_HEX(s[7])), \
       (RGBA_READ_HEX(s[0]) << 4) | (RGBA_READ_HEX(s[1])))

typedef void *UImage;
typedef void *UFont;

struct screen {
  int w;
  int h;
  UImage blit;
  char *pixels;
};

int init_screen(int w, int h);
void free_screen();
void refresh_screen();
struct screen *default_screen();

void set_cliprect(int x, int y, int w, int h);
void fill_rect(UImage dst, int x, int y, int w, int h, unsigned int c);
void draw_rect(UImage dst, int x, int y, int w, int h, unsigned int c);

UImage create_image(int w, int h, void *pixels);
void free_image(UImage img);
void *image_get_data(UImage img, int mutable);
void image_put_back_data(UImage img, void *data);
UImage resize_image(UImage img, int w, int h, int flags);
void blit_image(UImage dst, int dx, int dy, int dw, int dh,
                UImage src, int sx, int sy, int sw, int sh);

void draw_utf8(UImage dst, int x, int y, int c, UFont fnt, int len, char *s);
int get_utf8_size(UFont font, int len, char *str, int *w, int *h);

UFont create_font(const char *name, int size, const char *style);
void free_font(UFont font);
const char **font_list(int *n);
