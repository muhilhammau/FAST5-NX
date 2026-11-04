/* fast5_shdump.c -- the engine's shader sources carry a stray character after #endif
 * ("#endif]"): PowerVR drivers ignore it, Mesa's preprocessor rejects the whole shader.
 * Every source goes through here; junk after #endif / #else is cut. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <GLES2/gl2.h>
#include "util.h"

static void (*r_glShaderSource)(GLuint, GLsizei, const GLchar *const *, const GLint *);
static int g_log_budget = 6;

static int is_ident(int c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/* In place; returns the new length and counts the lines it cleaned. */
static size_t fix_source(char *s, size_t n, int *fixed) {
  size_t r = 0, w = 0;
  *fixed = 0;
  while (r < n) {
    size_t ls = r;
    while (r < n && s[r] != '\n') r++;
    size_t le = r;
    size_t ce = le;
    if (ce > ls && s[ce - 1] == '\r') ce--;
    size_t p = ls;
    while (p < ce && (s[p] == ' ' || s[p] == '\t')) p++;
    size_t keep = ce;
    if (p < ce && s[p] == '#') {
      p++;
      while (p < ce && (s[p] == ' ' || s[p] == '\t')) p++;
      size_t k = 0;
      if (ce - p >= 5 && !memcmp(s + p, "endif", 5)) k = 5;
      else if (ce - p >= 4 && !memcmp(s + p, "else", 4)) k = 4;
      if (k && (p + k == ce || !is_ident((unsigned char)s[p + k]))) {
        size_t q = p + k;
        while (q < ce && (s[q] == ' ' || s[q] == '\t')) q++;
        if (q < ce && !(s[q] == '/' && q + 1 < ce && (s[q + 1] == '/' || s[q + 1] == '*'))) {
          keep = p + k;
          (*fixed)++;
        }
      }
    }
    if (w != ls) memmove(s + w, s + ls, keep - ls);
    w += keep - ls;
    for (size_t i = ce; i < le; i++) s[w++] = s[i];
    if (le < n) s[w++] = '\n';
    r = le + 1;
  }
  return w;
}

static void w_glShaderSource(GLuint sh, GLsizei count, const GLchar *const *str, const GLint *len) {
  size_t total = 0;
  for (GLsizei i = 0; i < count; i++)
    if (str[i]) total += (len && len[i] >= 0) ? (size_t)len[i] : strlen(str[i]);
  char *all = (char *)malloc(total + 1);
  if (!all) {
    r_glShaderSource(sh, count, str, len);
    return;
  }
  size_t o = 0;
  for (GLsizei i = 0; i < count; i++) {
    if (!str[i]) continue;
    size_t l = (len && len[i] >= 0) ? (size_t)len[i] : strlen(str[i]);
    memcpy(all + o, str[i], l);
    o += l;
  }
  all[o] = 0;
  int fixed = 0;
  size_t n2 = fix_source(all, o, &fixed);
  all[n2] = 0;
  if (fixed && g_log_budget > 0) {
    g_log_budget--;
    debugPrintf("[shfix] shader %u: %d line(s) cleaned after #endif/#else (%u -> %u bytes)\n",
                (unsigned)sh, fixed, (unsigned)o, (unsigned)n2);
  }
  const GLchar *one = all;
  GLint l1 = (GLint)n2;
  r_glShaderSource(sh, 1, &one, &l1);
  free(all);
}

uintptr_t fast5_shdump_wrap(const char *name, uintptr_t real) {
  if (!strcmp(name, "glShaderSource")) {
    r_glShaderSource = (void (*)(GLuint, GLsizei, const GLchar *const *, const GLint *))real;
    return (uintptr_t)w_glShaderSource;
  }
  return 0;
}