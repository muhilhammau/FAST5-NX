#include "jni.h"
#include "port_config.h"
#include <stddef.h>

#define GLGAME "com/gameloft/android/GAND/GloftF5HP/ML/GLGame"

#include <switch.h>
#define F5H(fn) static jvalue fn(JObj *self, const jvalue *a, const JMethod *m)
#define AT "android/media/AudioTrack"
jvalue f5a_init(JObj *self, const jvalue *a, const JMethod *m);
jvalue f5a_write(JObj *self, const jvalue *a, const JMethod *m);
jvalue f5a_play(JObj *self, const jvalue *a, const JMethod *m);
jvalue f5a_pause(JObj *self, const jvalue *a, const JMethod *m);
jvalue f5a_stop(JObj *self, const jvalue *a, const JMethod *m);

F5H(f5_activity) { return jv_l(jni_singleton(GLGAME)); }

/* AudioTrack: a paced silent device until real output is wired to rt_audout.
 * <init>(streamType, rate, channelConfig, format, bufferSize, mode) */
__attribute__((unused)) F5H(f5_at_init_old) {
  self->v[0] = a[1].i;
  self->v[1] = (a[2].i == 12) ? 2 : 1; /* CHANNEL_OUT_STEREO = 12 */
  self->v[2] = (a[3].i == 3) ? 1 : 2;  /* ENCODING_PCM_8BIT = 3 */
  return jv_none();
}
F5H(f5_at_min_buffer) {
  jint ch = (a[1].i == 12) ? 2 : 1, bytes = (a[2].i == 3) ? 1 : 2;
  jint n = a[0].i * ch * bytes / 10; /* about 100 ms */
  return jv_i(n < 4096 ? 4096 : n);
}
__attribute__((unused)) F5H(f5_at_write_old) { /* ([BII)I: array, offset, length */
  jint len = a[2].i;
  int64_t bps = (int64_t)self->v[0] * self->v[1] * self->v[2];
  if (bps > 0 && len > 0)
    svcSleepThread((int64_t)len * 1000000000LL / bps);
  return jv_i(len);
}

#include <string.h>
F5H(f5_service) {
  const char *n = jni_utf(a[0].l);
  if (n && !strcmp(n, "phone"))
    return jv_l(jni_singleton("android/telephony/TelephonyManager"));
  return jv_l(jni_singleton("java/lang/Object"));
}
#include <stdlib.h>
/* Virtual AudioTrack: a buffer that drains in real time. write() returns at
 * once while the buffer has room and sleeps only for what does not fit. */
typedef struct { int rate, ch, bytes, cap; uint64_t end_ns; } F5At;
static void f5_at_free(JObj *o) { free(o->p); o->p = NULL; }
/* <init>(streamType, rate, channelConfig, format, bufferSize, mode) */
F5H(f5_at_init) {
  F5At *s = calloc(1, sizeof *s);
  if (s) {
    s->rate = a[1].i;
    s->ch = (a[2].i == 12 || a[2].i == 3) ? 2 : 1; /* new and old stereo constants */
    s->bytes = (a[3].i == 3) ? 1 : 2;              /* ENCODING_PCM_8BIT = 3 */
    s->cap = a[4].i;
  }
  self->p = s;
  self->finalize = f5_at_free;
  return jv_none();
}
F5H(f5_at_write) { /* ([BII)I: array, offset, length */
  F5At *s = self ? (F5At *)self->p : NULL;
  jint len = a[2].i;
  if (s && len > 0 && s->rate > 0) {
    int64_t bps = (int64_t)s->rate * s->ch * s->bytes;
    int64_t dur = (int64_t)len * 1000000000LL / bps;
    int64_t cap = (int64_t)(s->cap > 0 ? s->cap : 4096) * 1000000000LL / bps;
    uint64_t now = armTicksToNs(armGetSystemTick());
    if (s->end_ns < now) s->end_ns = now;
    s->end_ns += dur;
    if ((int64_t)(s->end_ns - now) > cap)
      svcSleepThread((int64_t)(s->end_ns - now) - cap);
  }
  return jv_i(len);
}
#include <string.h>
static JObj *f5_make_bytes(const char *s) {
  jsize n = (jsize)strlen(s);
  JObj *arr = jni_array('B', n);
  if (arr && n) memcpy(arr->a.data, s, (size_t)n);
  return arr;
}
F5H(f5_bytes_ver)  { return jv_l(f5_make_bytes("1.0.7")); }
F5H(f5_bytes_host) { return jv_l(f5_make_bytes("Switch")); }
F5H(f5_bytes_empty) { return jv_l(jni_array('B', 0)); }
static jvalue f5_bytes_db(JObj *self, const jvalue *a, const JMethod *m);
#include <stdio.h>
#include "dcr_path.h"
#include "util.h"
/* GLGame.db()[B: res/raw/igli.bin of the APK (test: read from the game folder). */
F5H(f5_db) {
  char path[512];
  snprintf(path, sizeof path, "%s/igli.bin", dcr_game_root());
  FILE *fp = fopen(path, "rb");
  if (!fp) {
    debugPrintf("[f5] db(): %s not found\n", path);
    return jv_l(NULL);
  }
  fseek(fp, 0, SEEK_END);
  long n = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  JObj *arr = (n > 0) ? jni_array('B', (jsize)n) : NULL;
  if (arr && fread(arr->a.data, 1, (size_t)n, fp) != (size_t)n)
    debugPrintf("[f5] db(): short read\n");
  fclose(fp);
  debugPrintf("[f5] db(): %ld bytes from %s\n", n, path);
  return jv_l(arr);
}
const JMethodDef jni_method_defs[] = {
    {GLGAME, "setSleep", NULL, jni_h_void},
    {GLGAME, "da", NULL, f5_activity},
    {AT, "<init>", NULL, f5a_init},
    {AT, "getMinBufferSize", NULL, f5_at_min_buffer},
    {AT, "write", NULL, f5a_write},
    {AT, "play", NULL, f5a_play},
    {AT, "pause", NULL, f5a_pause},
    {AT, "stop", NULL, f5a_stop},
    {AT, "release", NULL, jni_h_void},
    {GLGAME, "getSystemService", NULL, f5_service},
    {GLGAME, "getVersion", NULL, f5_bytes_ver},
    {GLGAME, "getHostName", NULL, f5_bytes_host},
    {GLGAME, "dc", NULL, f5_bytes_empty},
    {GLGAME, "db", NULL, f5_db},
    {GLGAME, "db", NULL, f5_bytes_db},
    {NULL, NULL, NULL, NULL}};

const JFieldDef jni_field_defs[] = {
    {"android/os/Build$VERSION", "SDK_INT", NULL, 19, NULL},
    {"android/content/Context", "WINDOW_SERVICE", NULL, 0, "window"},
    {"android/content/Context", "TELEPHONY_SERVICE", NULL, 0, "phone"},
    {NULL, NULL, NULL, 0, NULL}};

const char *const jni_class_supers[][2] = {{GLGAME, "android/app/Activity"}, {NULL, NULL}};

const char *const jni_missing_classes[] = {NULL};

#include "rt_apkfind.h"
#include "dcr_path.h"
#include <stdio.h>
#include <string.h>

static int f5_find_igli(const RtZipEntry *e, void *ctx) {
  if (strcmp(e->name, "res/raw/igli.bin") == 0) { *(RtZipEntry *)ctx = *e; return 1; }
  return 0;
}
F5H(f5_bytes_db) {
  RtZipEntry e; memset(&e, 0, sizeof e);
  const char *apk = dcr_apk_path();
  if (!apk[0]) return jv_l(jni_array('B', 0));
  rt_zip_walk_path(apk, f5_find_igli, &e);
  if (!e.name[0]) return jv_l(jni_array('B', 0));
  if (e.method != 0) return jv_l(jni_array('B', 0));
  FILE *f = fopen(apk, "rb");
  if (!f) return jv_l(jni_array('B', 0));
  uint64_t off = rt_zip_data_offset(f, 0, &e);
  JObj *arr = jni_array('B', (jsize)e.size);
  if (off && fseek(f, (long)off, SEEK_SET) == 0)
    if (fread(arr->a.data, 1, e.size, f) != e.size) memset(arr->a.data, 0, e.size);
  fclose(f);
  return jv_l(arr);
}
