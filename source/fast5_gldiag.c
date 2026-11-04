/* nm9_gldiag.c -- 9mm NX: why is the screen black? Counts draws, logs shader failures, samples pixels. */
#include <stdint.h>
#include <string.h>
#include <GLES2/gl2.h>
#include <switch.h>
#include "gl_layer.h"
#include "util.h"

static unsigned g_draws, g_clears, g_fb0, g_fbo;
static int g_sh_budget = 30;
static int g_vp_budget = 24;
static GLint g_vp[4];

static void (*r_glDrawElements)(GLenum, GLsizei, GLenum, const void *);
static void (*r_glDrawArrays)(GLenum, GLint, GLsizei);
static void (*r_glClear)(GLbitfield);
static void (*r_glViewport)(GLint, GLint, GLsizei, GLsizei);
static void (*r_glBindFramebuffer)(GLenum, GLuint);
static void (*r_glCompileShader)(GLuint);
static void (*r_glLinkProgram)(GLuint);

static void draw_tick(void) {
  unsigned v = __atomic_add_fetch(&g_draws, 1, __ATOMIC_RELAXED);
  if (v == 1 || v == 100 || v == 1000 || v == 10000 || v == 100000 || v == 1000000)
    debugPrintf("[diag] draws reached %u (clears %u, fb0 binds %u, fbo binds %u)\n", v, g_clears, g_fb0, g_fbo);
}
static void w_glDrawElements(GLenum m, GLsizei n, GLenum t, const void *p) {
  draw_tick();
  r_glDrawElements(m, n, t, p);
}
static void w_glDrawArrays(GLenum m, GLint f, GLsizei n) {
  draw_tick();
  r_glDrawArrays(m, f, n);
}
static void w_glClear(GLbitfield mask) {
  __atomic_add_fetch(&g_clears, 1, __ATOMIC_RELAXED);
  r_glClear(mask);
}
static void w_glViewport(GLint x, GLint y, GLsizei w, GLsizei h) {
  if (g_vp_budget > 0 && (g_vp[0] != x || g_vp[1] != y || g_vp[2] != w || g_vp[3] != h)) {
    g_vp_budget--;
    g_vp[0] = x; g_vp[1] = y; g_vp[2] = w; g_vp[3] = h;
    debugPrintf("[diag] glViewport %d %d %d %d\n", x, y, w, h);
  }
  r_glViewport(x, y, w, h);
}
static void w_glBindFramebuffer(GLenum t, GLuint f) {
  if (f == 0) __atomic_add_fetch(&g_fb0, 1, __ATOMIC_RELAXED);
  else __atomic_add_fetch(&g_fbo, 1, __ATOMIC_RELAXED);
  r_glBindFramebuffer(t, f);
}
static void w_glCompileShader(GLuint s) {
  r_glCompileShader(s);
  GLint ok = 1;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok && g_sh_budget > 0) {
    char log[768] = {0}, src[200] = {0};
    g_sh_budget--;
    glGetShaderInfoLog(s, sizeof log - 1, NULL, log);
    glGetShaderSource(s, sizeof src - 1, NULL, src);
    for (char *c = src; *c; c++) if (*c == '\n' || *c == '\r') *c = ' ';
    debugPrintf("[gl-shader] compile FAILED (shader %u): %s | source starts: %s\n", (unsigned)s, log, src);
  }
}
static void w_glLinkProgram(GLuint p) {
  r_glLinkProgram(p);
  GLint ok = 1;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok && g_sh_budget > 0) {
    char log[768] = {0};
    g_sh_budget--;
    glGetProgramInfoLog(p, sizeof log - 1, NULL, log);
    debugPrintf("[gl-shader] link FAILED (program %u): %s\n", (unsigned)p, log);
  }
}

#define WRAP(nm) \
  if (!strcmp(name, #nm)) { r_##nm = (__typeof__(r_##nm))real; return (uintptr_t)w_##nm; }

static uintptr_t nm9_diag_wrap(const char *name, uintptr_t real) {
  WRAP(glDrawElements)
  WRAP(glDrawArrays)
  WRAP(glClear)
  WRAP(glViewport)
  WRAP(glBindFramebuffer)
  WRAP(glCompileShader)
  WRAP(glLinkProgram)
  return 0;
}

void port_gl_before_swap(void) {
  static unsigned n, pd, pc, pf0, pfo;
  n++;
  if (!(n == 1 || n == 30 || n == 120 || n == 600 || n % 1800 == 0)) return;
  GLint fb = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fb);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  unsigned char px[9][4];
  memset(px, 0, sizeof px);
  static const int xs[3] = {160, 640, 1120};
  static const int ys[3] = {120, 360, 600};
  for (int j = 0; j < 3; j++)
    for (int i = 0; i < 3; i++)
      glReadPixels(xs[i], ys[j], 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px[j * 3 + i]);
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)fb);
  unsigned any = 0;
  for (int k = 0; k < 9; k++)
    if (px[k][0] | px[k][1] | px[k][2]) any++;
  debugPrintf("[diag] swap %u: +draws %u +clears %u +bind(fb0 %u, fbo %u) fbo-bound %d | nonblack %u/9 | center rgba %u,%u,%u,%u\n",
              n, g_draws - pd, g_clears - pc, g_fb0 - pf0, g_fbo - pfo, (int)fb, any,
              (unsigned)px[4][0], (unsigned)px[4][1], (unsigned)px[4][2], (unsigned)px[4][3]);
  pd = g_draws; pc = g_clears; pf0 = g_fb0; pfo = g_fbo;
}



/* The game draws but nothing presents it: present from the port loop when nativeRender did not swap. */
#include <EGL/egl.h>
EGLBoolean b_eglSwapBuffers(EGLDisplay d, EGLSurface s);
static unsigned g_fallback;
void nm9_present_fallback(uint32_t before) {
  if (dcr_gl_frames() != before) return;
  EGLDisplay d = eglGetCurrentDisplay();
  EGLSurface s = eglGetCurrentSurface(EGL_DRAW);
  if (d == EGL_NO_DISPLAY || s == EGL_NO_SURFACE) return;
  b_eglSwapBuffers(d, s);
  g_fallback++;
  if (g_fallback == 1 || g_fallback == 100 || g_fallback == 1000)
    debugPrintf("[diag] the game did not swap: presented %u frame(s) from the loop\n", g_fallback);
}

/* PVRTC (nm9_pvr.c) first, then the diagnostic wrapper around its result. */
uintptr_t fast5_pvr_wrap(const char *name, uintptr_t real);
uintptr_t fast5_diag_wrap(const char *name, uintptr_t real) {
  uintptr_t w = fast5_pvr_wrap(name, real);
  uintptr_t d = nm9_diag_wrap(name, w ? w : real);
  return d ? d : w;
}
