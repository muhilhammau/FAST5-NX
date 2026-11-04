/* Fast Five: __android_log_print without the engine's per-element XML trace.
 * The scene loader prints three trace lines for every XML token (tens of
 * thousands per race load); each one was a write to debug.log. They are
 * dropped here and counted; every other line is logged as before. The
 * runtime searches port_imports before its own table. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "imports.h"
#include "util.h"

static unsigned g_dropped;

static int noisy(const char *tag, const char *s) {
  if (!tag || strcmp(tag, "GLGame"))
    return 0;
  return !strncmp(s, "parseOpeningXMLElement", 22) || !strncmp(s, "parseClosingXMLElement", 22) ||
         !strncmp(s, "CXMLAttributesReaderUTF", 23);
}

static int f5_log_print(int prio, const char *tag, const char *fmt, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  if (n < 0)
    return n;
  if (noisy(tag, buf)) {
    if (++g_dropped % 5000 == 0)
      debugPrintf("[f5log] %u engine XML trace lines dropped\n", g_dropped);
    return n;
  }
  size_t l = strlen(buf);
  while (l && (buf[l - 1] == '\n' || buf[l - 1] == '\r'))
    buf[--l] = 0;
  debugPrintf("[%c/%s] %s\n", (prio >= 2 && prio <= 7) ? "VDIWEF"[prio - 2] : '?', tag ? tag : "", buf);
  return n;
}

const DynLibFunction port_imports[] = {{"__android_log_print", (uintptr_t)f5_log_print}};
const int port_imports_count = 1;