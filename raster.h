#define RGBA_A(c) (((c) >> 24) & 0xff)
#define RGBA_R(c) (((c) >> 16) & 0xff)
#define RGBA_G(c) (((c) >> 8) & 0xff)
#define RGBA_B(c) ((c) & 0xff)

#define RGBA(r, g, b, a) (((unsigned int)(a) << 24) \
                          | ((unsigned int)(r) << 16) \
                          | ((unsigned int)(g) << 8) \
                          | ((unsigned int)(b)))

#define RGBA_READ_HEX(x) (((x) >= '0' && (x) <= '9') \
                          ? (x) - '0' \
                          : ((x) >= 'a' && (x) <= 'f') ? (x) - 'a' + 10 : 0)

#define RGBA_FROM_STR8(s) \
  RGBA((RGBA_READ_HEX(s[0]) << 4) | (RGBA_READ_HEX(s[1])), \
       (RGBA_READ_HEX(s[2]) << 4) | (RGBA_READ_HEX(s[3])), \
       (RGBA_READ_HEX(s[4]) << 4) | (RGBA_READ_HEX(s[5])), \
       (RGBA_READ_HEX(s[6]) << 4) | (RGBA_READ_HEX(s[7])))

#define RGBA_FROM_STR6(s) \
  RGBA((RGBA_READ_HEX(s[0]) << 4) | (RGBA_READ_HEX(s[1])), \
       (RGBA_READ_HEX(s[2]) << 4) | (RGBA_READ_HEX(s[3])), \
       (RGBA_READ_HEX(s[4]) << 4) | (RGBA_READ_HEX(s[5])), \
       0xff)

#define RGBA_FROM_STR4(s) \
  RGBA(RGBA_READ_HEX(s[0]) << 4, \
       RGBA_READ_HEX(s[1]) << 4, \
       RGBA_READ_HEX(s[2]) << 4, \
       RGBA_READ_HEX(s[3]) << 4)

#define RGBA_FROM_STR3(s) \
  RGBA(RGBA_READ_HEX(s[0]) << 4, \
       RGBA_READ_HEX(s[1]) << 4, \
       RGBA_READ_HEX(s[2]) << 4, \
       0xff)

#define RGBA_FROM_STR2(s) RGBA(0xff, 0xff, 0xff, RGBA_READ_HEX(s[0]))
#define RGBA_FROM_STR1(s) RGBA(0xff, 0xff, 0xff, RGBA_READ_HEX(s[0]) << 4)

#define RGBA_VISIBLE(c) ((c) & 0xff000000)

void rgba_pixels_from_argb_image(unsigned int off_bytes,
                                 unsigned int len_bytes, unsigned int size,
                                 void *rgba, void *argb);

void rgba_pixels_to_argb_image(unsigned int off_bytes, unsigned int len_bytes,
                               unsigned int size, void *rgba, void *argb);
