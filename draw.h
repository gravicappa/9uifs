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

void fill_rect(Imlib_Image dst, int x, int y, int w, int h, unsigned int c);
void draw_rect(Imlib_Image dst, int x, int y, int w, int h, unsigned int c);
