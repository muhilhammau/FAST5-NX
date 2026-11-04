/* nm9_pvr.c -- PVRTC textures on a GPU that has none.
 *
 * 9mm's data holds its world textures as PVRTC 4bpp (PVR! headers in the .gla
 * files). Mesa's nouveau on the Switch offers S3TC, ASTC and ETC1, not
 * GL_IMG_texture_compression_pvrtc, so the engine never even tried them
 * (the walls come out magenta). Two wrappers, hooked through the runtime's
 * port_gl_wrap (gl_mesa.c):
 *   glGetString(GL_EXTENSIONS)   adds GL_IMG_texture_compression_pvrtc
 *   glCompressedTexImage2D /     PVRTC formats (0x8C00-0x8C03) are decoded on
 *   glCompressedTexSubImage2D    the CPU to RGBA8888 and uploaded as a plain
 *                                texture; every other format goes through.
 * The decoder is the PowerVR PVRTC1 4bpp one: words in Morton order, two
 * colours per 4x4 block interpolated bilinearly between block centres,
 * 2-bit modulation per pixel (0, 3, 5, 8 of 8; punch-through mode 0, 4, 4+
 * transparent, 8). 2bpp is not decoded (grey, logged once). MIT.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <GLES2/gl2.h>

#include "util.h"

#define PVR_RGB_4BPP 0x8C00
#define PVR_RGB_2BPP 0x8C01
#define PVR_RGBA_4BPP 0x8C02
#define PVR_RGBA_2BPP 0x8C03
#define IS_PVR(f) ((f) >= PVR_RGB_4BPP && (f) <= PVR_RGBA_2BPP)

typedef struct {
  uint8_t a[4], b[4]; /* the two colours, RGBA 8 bit */
  uint32_t mod;
  uint8_t punch;
} PvrWord;

static uint32_t twiddle(uint32_t ys, uint32_t xs, uint32_t y, uint32_t x) {
  uint32_t mind = xs, maxv = y, tw = 0, src = 1, dst = 1, sh = 0;
  if (ys < xs) {
    mind = ys;
    maxv = x;
  }
  while (src < mind) {
    if (y & src)
      tw |= dst;
    if (x & src)
      tw |= dst << 1;
    src <<= 1;
    dst <<= 2;
    sh++;
  }
  maxv >>= sh;
  tw |= maxv << (2 * sh);
  return tw;
}

static inline uint8_t e5(unsigned v) { return (uint8_t)((v << 3) | (v >> 2)); }
static inline uint8_t e4(unsigned v) { return (uint8_t)(v * 17); }
static inline uint8_t e3(unsigned v) { return (uint8_t)((v << 5) | (v << 2) | (v >> 1)); }

static void unpack_colors(uint32_t cd, PvrWord *w) {
  unsigned lo = cd & 0xFFFF, hi = cd >> 16;
  if (lo & 0x8000) { /* A opaque: R5 G5 B4 */
    w->a[0] = e5((lo >> 10) & 31);
    w->a[1] = e5((lo >> 5) & 31);
    unsigned b4 = (lo >> 1) & 15;
    w->a[2] = e5((b4 << 1) | (b4 >> 3));
    w->a[3] = 255;
  } else { /* A: A3 R4 G4 B3 */
    w->a[3] = e3((lo >> 12) & 7);
    w->a[0] = e4((lo >> 8) & 15);
    w->a[1] = e4((lo >> 4) & 15);
    w->a[2] = e3((lo >> 1) & 7);
  }
  if (hi & 0x8000) { /* B opaque: R5 G5 B5 */
    w->b[0] = e5((hi >> 10) & 31);
    w->b[1] = e5((hi >> 5) & 31);
    w->b[2] = e5(hi & 31);
    w->b[3] = 255;
  } else { /* B: A3 R4 G4 B4 */
    w->b[3] = e3((hi >> 12) & 7);
    w->b[0] = e4((hi >> 8) & 15);
    w->b[1] = e4((hi >> 4) & 15);
    w->b[2] = e4(hi & 15);
  }
}

/* Returns 0 when the data is too short. dst: w*h RGBA. */
static int decode_pvrtc4(const uint8_t *src, size_t size, int w, int h, int has_alpha, uint8_t *dst) {
  int bw = (w + 3) / 4, bh = (h + 3) / 4;
  if (bw < 2)
    bw = 2;
  if (bh < 2)
    bh = 2;
  if (size < (size_t)bw * (size_t)bh * 8)
    return 0;
  PvrWord *ws = (PvrWord *)malloc((size_t)bw * (size_t)bh * sizeof(PvrWord));
  if (!ws)
    return 0;
  for (int y = 0; y < bh; y++)
    for (int x = 0; x < bw; x++) {
      uint32_t idx = twiddle((uint32_t)bh, (uint32_t)bw, (uint32_t)y, (uint32_t)x);
      uint32_t mod, cd;
      memcpy(&mod, src + (size_t)idx * 8, 4);
      memcpy(&cd, src + (size_t)idx * 8 + 4, 4);
      PvrWord *p = &ws[y * bw + x];
      p->mod = mod;
      p->punch = (uint8_t)(cd & 1);
      unpack_colors(cd, p);
    }
  static const uint8_t k_std[4] = {0, 3, 5, 8};
  static const uint8_t k_punch[4] = {0, 4, 4, 8};
  for (int y = 0; y < h; y++) {
    int v = y + 2, wy1 = (v >> 2) % bh, wy0 = ((v >> 2) - 1 + bh) % bh, ty = v & 3;
    for (int x = 0; x < w; x++) {
      int u = x + 2, wx1 = (u >> 2) % bw, wx0 = ((u >> 2) - 1 + bw) % bw, tx = u & 3;
      const PvrWord *p0 = &ws[wy0 * bw + wx0], *p1 = &ws[wy0 * bw + wx1];
      const PvrWord *p2 = &ws[wy1 * bw + wx0], *p3 = &ws[wy1 * bw + wx1];
      const PvrWord *own = &ws[(y >> 2) * bw + (x >> 2)];
      unsigned bits = (own->mod >> (2 * ((y & 3) * 4 + (x & 3)))) & 3;
      unsigned m = own->punch ? k_punch[bits] : k_std[bits];
      int clear = own->punch && bits == 2;
      uint8_t *o = dst + ((size_t)y * (size_t)w + (size_t)x) * 4;
      for (int c = 0; c < 4; c++) {
        int pa = ((p0->a[c] * (4 - tx) + p1->a[c] * tx) * (4 - ty) + (p2->a[c] * (4 - tx) + p3->a[c] * tx) * ty);
        int pb = ((p0->b[c] * (4 - tx) + p1->b[c] * tx) * (4 - ty) + (p2->b[c] * (4 - tx) + p3->b[c] * tx) * ty);
        o[c] = (uint8_t)((pa * (int)(8 - m) + pb * (int)m) / 128); /* 16 (bilinear) * 8 (modulation) */
      }
      if (!has_alpha)
        o[3] = 255;
      else if (clear)
        o[3] = 0;
    }
  }
  free(ws);
  return 1;
}

/* ------------------------------------------------------------ the wrappers */
static const GLubyte *(*r_glGetString)(GLenum);
static void (*r_glCompressedTexImage2D)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *);
static void (*r_glCompressedTexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei,
                                           const void *);
static unsigned g_uploads, g_us_total;

static const GLubyte *w_glGetString(GLenum name) {
  const GLubyte *s = r_glGetString(name);
  if (name != GL_EXTENSIONS || !s)
    return s;
  static char *buf;
  if (!buf) {
    size_t n = strlen((const char *)s) + 64;
    buf = (char *)malloc(n);
    if (!buf)
      return s;
    /* In front, not behind: an engine that copies the list into a fixed
     * buffer would cut the tail. */
    snprintf(buf, n, "GL_IMG_texture_compression_pvrtc %s", (const char *)s);
    debugPrintf("[pvr] GL_EXTENSIONS: GL_IMG_texture_compression_pvrtc added (in front)\n");
  }
  return (const GLubyte *)buf;
}

/* An engine may take the formats from GL_COMPRESSED_TEXTURE_FORMATS instead
 * of the extension string (it logs "Setting EPF_..." for the ones it knows):
 * the four PVRTC enums are appended to that list too. */
static void (*r_glGetIntegerv)(GLenum, GLint *);
static void w_glGetIntegerv(GLenum p, GLint *v) {
  r_glGetIntegerv(p, v);
  if (!v)
    return;
  if (p == 0x86A2) { /* GL_NUM_COMPRESSED_TEXTURE_FORMATS */
    static int logged;
    *v += 4;
    if (!logged++)
      debugPrintf("[pvr] GL_NUM_COMPRESSED_TEXTURE_FORMATS -> %d (4 PVRTC added)\n", (int)*v);
  } else if (p == 0x86A3) { /* GL_COMPRESSED_TEXTURE_FORMATS */
    GLint n = 0;
    r_glGetIntegerv(0x86A2, &n);
    v[n] = PVR_RGB_4BPP;
    v[n + 1] = PVR_RGB_2BPP;
    v[n + 2] = PVR_RGBA_4BPP;
    v[n + 3] = PVR_RGBA_2BPP;
  }
}

/* 1: handled (decoded and uploaded). */
static int pvr_decode_to(GLenum fmt, int w, int h, int size, const void *d, uint8_t **out) {
  if (!d || w <= 0 || h <= 0)
    return 0;
  uint8_t *rgba = (uint8_t *)malloc((size_t)w * (size_t)h * 4);
  if (!rgba)
    return 0;
  const u64 t0 = armGetSystemTick();
  if (fmt == PVR_RGB_2BPP || fmt == PVR_RGBA_2BPP) {
    static int warned;
    if (!warned++)
      debugPrintf("[pvr] 2bpp PVRTC is not decoded (grey)\n");
    memset(rgba, 0x80, (size_t)w * (size_t)h * 4);
  } else if (!decode_pvrtc4((const uint8_t *)d, (size_t)size, w, h, fmt == PVR_RGBA_4BPP, rgba)) {
    debugPrintf("[pvr] 0x%x %dx%d: %d bytes is not enough\n", (unsigned)fmt, w, h, size);
    free(rgba);
    return 0;
  }
  g_us_total += (unsigned)(armTicksToNs(armGetSystemTick() - t0) / 1000);
  *out = rgba;
  return 1;
}

static void w_glCompressedTexImage2D(GLenum t, GLint lvl, GLenum ifmt, GLsizei w, GLsizei h, GLint border,
                                     GLsizei size, const void *d) {
  if (IS_PVR(ifmt)) {
    uint8_t *rgba = NULL;
    if (pvr_decode_to(ifmt, w, h, size, d, &rgba)) {
      g_uploads++;
      if (g_uploads <= 25 || g_uploads == 100 || g_uploads == 500 || g_uploads == 2000)
        debugPrintf("[pvr] #%u 0x%x %dx%d lvl %d (%d bytes), decoding total %u ms\n", g_uploads, (unsigned)ifmt,
                    (int)w, (int)h, (int)lvl, (int)size, g_us_total / 1000);
      glTexImage2D(t, lvl, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
      free(rgba);
      return;
    }
  }
  {
    static unsigned other;
    if (other++ < 15)
      debugPrintf("[pvr] other compressed upload 0x%x %dx%d lvl %d (%d bytes)\n", (unsigned)ifmt, (int)w, (int)h,
                  (int)lvl, (int)size);
  }
  r_glCompressedTexImage2D(t, lvl, ifmt, w, h, border, size, d);
}

static void w_glCompressedTexSubImage2D(GLenum t, GLint lvl, GLint xo, GLint yo, GLsizei w, GLsizei h, GLenum fmt,
                                        GLsizei size, const void *d) {
  if (IS_PVR(fmt)) {
    uint8_t *rgba = NULL;
    if (pvr_decode_to(fmt, w, h, size, d, &rgba)) {
      glTexSubImage2D(t, lvl, xo, yo, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
      free(rgba);
      return;
    }
  }
  r_glCompressedTexSubImage2D(t, lvl, xo, yo, w, h, fmt, size, d);
}

/* Called by port_gl_wrap for each gl* name: the wrapper, or 0. */
uintptr_t fast5_pvr_wrap(const char *name, uintptr_t real) {
  if (!real || !name)
    return 0;
  if (!strcmp(name, "glGetString")) {
    r_glGetString = (const GLubyte * (*)(GLenum))real;
    return (uintptr_t)w_glGetString;
  }
  if (!strcmp(name, "glGetIntegerv")) {
    r_glGetIntegerv = (void (*)(GLenum, GLint *))real;
    return (uintptr_t)w_glGetIntegerv;
  }
  if (!strcmp(name, "glCompressedTexImage2D")) {
    r_glCompressedTexImage2D = (void (*)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *))real;
    return (uintptr_t)w_glCompressedTexImage2D;
  }
  if (!strcmp(name, "glCompressedTexSubImage2D")) {
    r_glCompressedTexSubImage2D =
        (void (*)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void *))real;
    return (uintptr_t)w_glCompressedTexSubImage2D;
  }
  return 0;
}
