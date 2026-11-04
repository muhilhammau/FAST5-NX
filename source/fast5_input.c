/* Fast Five: the Switch's touch screen and pad into the game's own natives.
 * Touch: GLGame.nativeTouchPressed/Moved/Released(III)V.
 * Pad: buttons as Android key codes (nativeSetOnKeyDown/Up(I)V); the left stick
 * is sent as a tilt to nativeAccelerometer(FFF)V, for steering.
 * All of the mappings below are first guesses: the log says what was sent. */
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <switch.h>
#include <ctype.h>
#include <stdio.h>
#include "dcr_path.h"

#include "port_config.h"
#include "rt_pad.h"
#include "rt_window.h"
#include "util.h"

/* define F5_TOUCH_ID_FIRST in port_config.h if the natives take (id, x, y) */
#ifdef F5_TOUCH_ID_FIRST
#define TCALL(f, id, x, y) f(g_env, g_cls, (id), (x), (y))
#else
#define TCALL(f, id, x, y) f(g_env, g_cls, (x), (y), (id))
#endif
/* which argument of nativeAccelerometer carries the steering tilt, and its sign */
#ifndef F5_TILT_SIGN
#define F5_TILT_SIGN 1.0f
#endif

#define TOUCH_SLOTS 10
typedef void (*fn_i)(void *, void *, int);
typedef void (*fn_iii)(void *, void *, int, int, int);
typedef void (*fn_fff)(void *, void *, float, float, float);

static void f5_load_buttons(void);
static void *g_env, *g_cls;
static fn_i key_down, key_up;
static fn_iii t_press, t_move, t_release;
static fn_fff accel;
static PadState pads[2];
static int ready, log_left = 400;
static u64 prev_buttons;
static struct { int used; u32 fid; int x, y; } ts[TOUCH_SLOTS];

static const struct { u64 mask; int code; } kmap[] = {
    {HidNpadButton_Up, 19},   {HidNpadButton_Down, 20}, {HidNpadButton_Left, 21}, {HidNpadButton_Right, 22},
    {HidNpadButton_A, 23},    {HidNpadButton_B, 4},     {HidNpadButton_X, 99},    {HidNpadButton_Y, 100},
    {HidNpadButton_L, 102},   {HidNpadButton_R, 103},   {HidNpadButton_ZL, 104},  {HidNpadButton_ZR, 105},
    {HidNpadButton_Plus, 108}, {HidNpadButton_Minus, 109},
};

void f5_input_init(void *env, void *cls, void *kd, void *ku, void *tp, void *tm, void *tr, void *ac) {
  g_env = env;
  g_cls = cls;
  key_down = (fn_i)kd;
  key_up = (fn_i)ku;
  t_press = (fn_iii)tp;
  t_move = (fn_iii)tm;
  t_release = (fn_iii)tr;
  accel = (fn_fff)ac;
  hidInitializeTouchScreen();
  rt_pad_setup(1, 1);
  rt_pad_slot(&pads[0], 0);
  rt_pad_slot(&pads[1], RT_PAD_HANDHELD);
  ready = 1;
  f5_load_buttons();
  debugPrintf("[input] touch + pad bound\n");
}

/* ---- pad buttons that press the race HUD buttons (touch positions on the 1280x720 screen) ---- */
#define V_HOLD 3
#define NV 8
typedef struct { const char *name; u64 mask; int x, y, hold; } VBtn;
static VBtn vb[NV] = {
    {"PAUSE", HidNpadButton_Plus, 50, 34, 0},
    {"CAMERA", HidNpadButton_Y, 1230, 36, 0},
    {"TIMER", HidNpadButton_L | HidNpadButton_ZL, 52, 272, 0},
    {"TURBO", HidNpadButton_A | HidNpadButton_R | HidNpadButton_ZR, 1228, 272, 0},
    {"EXTRA1", HidNpadButton_X, -1, -1, 0},
    {"EXTRA2", HidNpadButton_Minus, -1, -1, 0},
    {"LEFT", 0, -1, -1, 0},
    {"RIGHT", 0, -1, -1, 0},
};
static float g_tilt_sign = F5_TILT_SIGN, g_tilt_gain = 0.5f;
static float g_stick_x;
static int acc_log;

static void f5_load_buttons(void) {
  char path[512];
  snprintf(path, sizeof path, "%s/buttons.txt", dcr_game_root());
  FILE *f = fopen(path, "r");
  if (!f) {
    debugPrintf("[input] %s not found: default button positions\n", path);
  } else {
    char line[128];
    while (fgets(line, sizeof line, f)) {
      char nm[24];
      if (line[0] == '#' || sscanf(line, "%23s", nm) != 1)
        continue;
      for (char *c = nm; *c; c++)
        *c = (char)toupper((unsigned char)*c);
      if (!strcmp(nm, "TILT_SIGN") || !strcmp(nm, "TILT_GAIN")) {
        float v;
        if (sscanf(line, "%*s %f", &v) == 1) {
          if (nm[5] == 'S') g_tilt_sign = v;
          else g_tilt_gain = v;
        }
        continue;
      }
      int x, y;
      if (sscanf(line, "%*s %d %d", &x, &y) != 2)
        continue;
      for (int i = 0; i < NV; i++)
        if (!strcmp(nm, vb[i].name)) {
          vb[i].x = x;
          vb[i].y = y;
        }
    }
    fclose(f);
    debugPrintf("[input] loaded %s\n", path);
  }
  for (int i = 0; i < NV; i++)
    debugPrintf("[input] %s -> %d,%d\n", vb[i].name, vb[i].x, vb[i].y);
  debugPrintf("[input] tilt sign %d gain x100 %d\n", (int)g_tilt_sign, (int)(g_tilt_gain * 100.0f));
}

typedef struct { u32 fid; int x, y; } Cur;

static void poll_touch(void) {
  HidTouchScreenState st;
  Cur cur[TOUCH_SLOTS];
  int n = 0, seen[TOUCH_SLOTS] = {0}, gw = 1280, gh = 720;
  memset(&st, 0, sizeof st);
  dcr_window_size(&gw, &gh); /* the game's screen; the touch screen is 1280x720 */
  if (hidGetTouchScreenStates(&st, 1) >= 1) {
    for (int i = 0; i < (int)st.count && i < 4 && n < TOUCH_SLOTS; i++) {
      cur[n].fid = st.touches[i].finger_id;
      cur[n].x = (int)((int64_t)st.touches[i].x * gw / 1280);
      cur[n].y = (int)((int64_t)st.touches[i].y * gh / 720);
      n++;
    }
  }
  for (int i = 0; i < NV; i++) {
    if (vb[i].x < 0)
      continue;
    if (prev_buttons & vb[i].mask)
      vb[i].hold = V_HOLD;
    if (i == NV - 2 && g_stick_x < -0.35f)
      vb[i].hold = V_HOLD;
    if (i == NV - 1 && g_stick_x > 0.35f)
      vb[i].hold = V_HOLD;
    if (vb[i].hold > 0) {
      vb[i].hold--;
      if (n < TOUCH_SLOTS) {
        cur[n].fid = 0xFD00u + (u32)i;
        cur[n].x = (int)((int64_t)vb[i].x * gw / 1280);
        cur[n].y = (int)((int64_t)vb[i].y * gh / 720);
        n++;
      }
    }
  }
  for (int i = 0; i < n; i++) {
    int s = -1;
    for (int k = 0; k < TOUCH_SLOTS; k++)
      if (ts[k].used && ts[k].fid == cur[i].fid) {
        s = k;
        break;
      }
    if (s < 0) {
      for (int k = 0; k < TOUCH_SLOTS; k++)
        if (!ts[k].used) {
          s = k;
          break;
        }
      if (s < 0)
        continue;
      ts[s].used = 1;
      ts[s].fid = cur[i].fid;
      ts[s].x = cur[i].x;
      ts[s].y = cur[i].y;
      seen[s] = 1;
      if (log_left > 0) {
        log_left--;
        debugPrintf("[input] touch down slot %d at %d,%d (id 0x%x)\n", s, cur[i].x, cur[i].y, (unsigned)cur[i].fid);
      }
      TCALL(t_press, s, cur[i].x, cur[i].y);
    } else {
      seen[s] = 1;
      if (cur[i].x != ts[s].x || cur[i].y != ts[s].y) {
        ts[s].x = cur[i].x;
        ts[s].y = cur[i].y;
        TCALL(t_move, s, cur[i].x, cur[i].y);
      }
    }
  }
  for (int k = 0; k < TOUCH_SLOTS; k++)
    if (ts[k].used && !seen[k]) {
      ts[k].used = 0;
      TCALL(t_release, k, ts[k].x, ts[k].y);
    }
}
static void poll_pad(void) {
  float st[2][4] = {{0}};
  u64 b = 0;
  float sx = 0.0f;
  for (int i = 0; i < 2; i++) {
    padUpdate(&pads[i]);
    b |= rt_pad_read(&pads[i], st[i]);
    if (fabsf(st[i][0]) > fabsf(sx))
      sx = st[i][0];
  }
  if (fabsf(sx) < 0.15f)
    sx = 0.0f;
  g_stick_x = sx;
  { static int sl = 20; if (fabsf(sx) > 0.35f && sl > 0) { sl--; debugPrintf("[input] stick x*100 = %d\n", (int)(sx * 100.0f)); } }
  u64 down = b & ~prev_buttons, up = prev_buttons & ~b;
  for (unsigned i = 0; i < sizeof kmap / sizeof kmap[0]; i++) {
    if (down & kmap[i].mask) {
      if (log_left > 0) {
        log_left--;
        debugPrintf("[input] key down %d\n", kmap[i].code);
      }
      key_down(g_env, g_cls, kmap[i].code);
    }
    if (up & kmap[i].mask)
      key_up(g_env, g_cls, kmap[i].code);
  }
  prev_buttons = b;
  /* landscape device held upright: gravity along x; steering rotates it about
   * the screen normal, which moves gravity into y */
  float th = sx * g_tilt_gain;
  float ay = g_tilt_sign * 9.81f * sinf(th);
  if (fabsf(sx) > 0.3f && acc_log < 12) {
    acc_log++;
    debugPrintf("[input] steer: stick %d%%, accel y x100 = %d\n", (int)(sx * 100.0f), (int)(ay * 100.0f));
  }
  accel(g_env, g_cls, 9.81f * cosf(th), ay, 0.0f);
}

void f5_input_poll(void) {
  if (!ready)
    return;
  poll_touch();
  poll_pad();
}