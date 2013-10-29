#include <stdint.h>

void
rgba_pixels_from_argb_image(unsigned int off_bytes, unsigned int len_bytes,
                            unsigned int size, void *rgba, void *argb)
{
  uint32_t *src, pix;
  unsigned char *dst = rgba;

  if (size <= off_bytes)
    return;
  if (off_bytes + len_bytes >= size)
    len_bytes = size - off_bytes;
  src = (uint32_t *)argb + (off_bytes >> 2);
  pix = *src;
  switch (off_bytes & 3) {
  case 1:
    if (len_bytes-- > 0)
      *dst++ = (pix >> 8) & 0xff;
  case 2:
    if (len_bytes-- > 0)
      *dst++ = pix & 0xff;
  case 3:
    if (len_bytes-- > 0)
      *dst++ = pix >> 24;
  }
  if (off_bytes & 3)
    pix = *(++src);
  for (; len_bytes >= 4; pix = *src++, len_bytes -= 4) {
    *dst++ = (pix >> 16) & 0xff;
    *dst++ = (pix >> 8) & 0xff;
    *dst++ = pix & 0xff;
    *dst++ = pix >> 24;
  }
  if (len_bytes)
    pix = *src;
  switch (len_bytes) {
  case 3: dst[2] = pix & 0xff;
  case 2: dst[1] = (pix >> 8) & 0xff;
  case 1: dst[0] = (pix >> 16) & 0xff;
  }
}

void
rgba_pixels_to_argb_image(unsigned int off_bytes, unsigned int len_bytes,
                          unsigned int size, void *rgba, void *argb)
{
  uint32_t *dst, pix;
  unsigned char *src = rgba;

  if (size <= off_bytes)
    return;
  if (off_bytes + len_bytes >= size)
    len_bytes = size - off_bytes;
  dst = (uint32_t *)argb + (off_bytes >> 2);
  pix = *dst;
  switch (off_bytes & 3) {
  case 1:
    if (len_bytes-- > 0)
      pix = (pix & 0xffff00ff) | (*src++ << 8);
  case 2:
    if (len_bytes-- > 0)
      pix = (pix & 0xffffff00) | *src++;
  case 3:
    if (len_bytes-- > 0)
      pix = (pix & 0x00ffffff) | (*src++ << 24);
  }
  if (off_bytes & 3)
    *dst++ = pix;
  for (; len_bytes >= 4; len_bytes -= 4, src += 4, ++dst)
    *dst = src[2] | (src[1] << 8) | (src[0] << 16) | (src[3] << 24);
  if (len_bytes) {
    pix = *dst;
    switch (len_bytes) {
    case 3: pix = (pix & 0xffffff00) | src[2];
    case 2: pix = (pix & 0xffff00ff) | (src[1] << 8);
    case 1: pix = (pix & 0xff00ffff) | (src[0] << 16);
    }
    *dst = pix;
  }
}
