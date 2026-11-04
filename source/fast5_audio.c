/* Fast Five: the game's java AudioTrack, played through the runtime's audout
 * pump. Each AudioTrack has a ring of stereo s16 frames at 48 kHz; write()
 * converts the game's PCM (8/16 bit, mono/stereo, any rate) into it and blocks
 * while it is full, like a real track; the pump thread mixes every playing
 * track into audout. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include "jni.h"
#include "rt_audout.h"
#include "util.h"

#define RING_FRAMES 8192u /* per track: 170 ms at 48 kHz; a power of two */
#define MAX_TRACKS 6
#define OUT_RATE 48000u

typedef struct {
  int rate, ch, bytes;
  volatile int playing;
  uint32_t step;      /* source frames to output frames, 16.16: rate / 48000 */
  uint32_t ph;        /* position between the previous and the next source frame, 16.16 */
  int pl, pr, have_prev;
  volatile uint32_t rd, wr; /* frame counters; wr by the game thread, rd by the pump */
  int16_t *ring;
} Track;

static Mutex g_mx;
static Track *g_tracks[MAX_TRACKS];
static int g_started, g_inited;
static int32_t g_mix[2 * RT_AUDOUT_FRAMES];

static void f5_fill(int16_t *out, int frames, void *ud) {
  (void)ud;
  if (frames > (int)RT_AUDOUT_FRAMES)
    frames = (int)RT_AUDOUT_FRAMES;
  memset(g_mix, 0, (size_t)frames * 2 * sizeof g_mix[0]);
  mutexLock(&g_mx);
  for (int k = 0; k < MAX_TRACKS; k++) {
    Track *t = g_tracks[k];
    if (!t || !t->playing)
      continue;
    uint32_t rd = t->rd;
    uint32_t avail = __atomic_load_n(&t->wr, __ATOMIC_ACQUIRE) - rd;
    if (avail > (uint32_t)frames)
      avail = (uint32_t)frames;
    for (uint32_t i = 0; i < avail; i++) {
      uint32_t ix = (rd + i) & (RING_FRAMES - 1);
      g_mix[2 * i] += t->ring[2 * ix];
      g_mix[2 * i + 1] += t->ring[2 * ix + 1];
    }
    __atomic_store_n(&t->rd, rd + avail, __ATOMIC_RELEASE);
  }
  mutexUnlock(&g_mx);
  for (int i = 0; i < frames * 2; i++) {
    int32_t v = g_mix[i];
    out[i] = (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
  }
}

static void track_free(JObj *o) {
  Track *t = (Track *)o->p;
  if (!t)
    return;
  mutexLock(&g_mx);
  for (int k = 0; k < MAX_TRACKS; k++)
    if (g_tracks[k] == t)
      g_tracks[k] = NULL;
  mutexUnlock(&g_mx);
  free(t->ring);
  free(t);
  o->p = NULL;
}

/* <init>(streamType, rate, channelConfig, format, bufferSize, mode) */
jvalue f5a_init(JObj *self, const jvalue *a, const JMethod *m) {
  (void)m;
  Track *t = calloc(1, sizeof *t);
  int16_t *ring = t ? calloc(RING_FRAMES * 2, sizeof(int16_t)) : NULL;
  if (!t || !ring) {
    free(t);
    free(ring);
    self->p = NULL;
    return jv_none();
  }
  t->ring = ring;
  t->rate = a[1].i > 0 ? a[1].i : 44100;
  t->ch = (a[2].i == 12 || a[2].i == 3) ? 2 : 1; /* new and old stereo constants */
  t->bytes = (a[3].i == 3) ? 1 : 2;              /* ENCODING_PCM_8BIT = 3 */
  t->step = (uint32_t)(((uint64_t)t->rate << 16) / OUT_RATE);
  if (!g_inited) {
    mutexInit(&g_mx);
    g_inited = 1;
  }
  int slot = -1;
  mutexLock(&g_mx);
  for (int k = 0; k < MAX_TRACKS; k++)
    if (!g_tracks[k]) {
      slot = k;
      g_tracks[k] = t;
      break;
    }
  mutexUnlock(&g_mx);
  if (!g_started && rt_audout_open() == 0 && rt_audout_pump_start(f5_fill, NULL, 0x2A, -2) == 0)
    g_started = 1;
  debugPrintf("[audio] AudioTrack %d Hz, %d ch, %d-bit, slot %d, pump %s\n", t->rate, t->ch, t->bytes * 8, slot,
              g_started ? "running" : "NOT running");
  self->p = t;
  self->finalize = track_free;
  return jv_none();
}

static void push(Track *t, int l, int r) {
  for (;;) {
    uint32_t wr = t->wr;
    if (wr - __atomic_load_n(&t->rd, __ATOMIC_ACQUIRE) < RING_FRAMES) {
      uint32_t ix = wr & (RING_FRAMES - 1);
      t->ring[2 * ix] = (int16_t)l;
      t->ring[2 * ix + 1] = (int16_t)r;
      __atomic_store_n(&t->wr, wr + 1, __ATOMIC_RELEASE);
      return;
    }
    if (!t->playing)
      return; /* nothing drains it: drop */
    svcSleepThread(1000000);
  }
}

/* ([BII)I: array, offset, length in bytes */
jvalue f5a_write(JObj *self, const jvalue *a, const JMethod *m) {
  (void)m;
  Track *t = self ? (Track *)self->p : NULL;
  JObj *arr = (JObj *)a[0].l;
  jint off = a[1].i, len = a[2].i;
  if (!t || !arr || !arr->a.data || len <= 0 || off < 0)
    return jv_i(len > 0 ? len : 0);
  if (off + len > arr->a.len)
    len = arr->a.len > off ? arr->a.len - off : 0;
  const uint8_t *src = (const uint8_t *)arr->a.data + off;
  const int fs = t->ch * t->bytes;
  if (!g_started) { /* no pump: only pace the game like a silent device */
    int64_t bps = (int64_t)t->rate * fs;
    if (bps > 0)
      svcSleepThread((int64_t)len * 1000000000LL / bps);
    return jv_i(len);
  }
  for (jint i = 0; i + fs <= len; i += fs) {
    int l, r;
    if (t->bytes == 2) {
      l = (int16_t)(src[i] | (src[i + 1] << 8));
      r = (t->ch == 2) ? (int16_t)(src[i + 2] | (src[i + 3] << 8)) : l;
    } else {
      l = ((int)src[i] - 128) << 8;
      r = (t->ch == 2) ? (((int)src[i + 1] - 128) << 8) : l;
    }
    if (!t->have_prev) {
      t->pl = l;
      t->pr = r;
      t->have_prev = 1;
    }
    while (t->ph < 65536u) {
      int ol = t->pl + (int)(((int64_t)(l - t->pl) * (int)t->ph) >> 16);
      int orr = t->pr + (int)(((int64_t)(r - t->pr) * (int)t->ph) >> 16);
      push(t, ol, orr);
      t->ph += t->step;
    }
    t->ph -= 65536u;
    t->pl = l;
    t->pr = r;
  }
  return jv_i(len);
}

jvalue f5a_play(JObj *self, const jvalue *a, const JMethod *m) {
  (void)a; (void)m;
  if (self && self->p)
    ((Track *)self->p)->playing = 1;
  return jv_none();
}

jvalue f5a_pause(JObj *self, const jvalue *a, const JMethod *m) {
  (void)a; (void)m;
  if (self && self->p)
    ((Track *)self->p)->playing = 0;
  return jv_none();
}

jvalue f5a_stop(JObj *self, const jvalue *a, const JMethod *m) {
  (void)a; (void)m;
  Track *t = self ? (Track *)self->p : NULL;
  if (t) {
    t->playing = 0;
    mutexLock(&g_mx);
    t->rd = t->wr; /* discard what was queued */
    mutexUnlock(&g_mx);
  }
  return jv_none();
}

/* HOME: the pump stops filling and the voices hold where they are */
void f5_audio_pause(int on) {
  if (g_started)
    rt_audout_pause(on);
}