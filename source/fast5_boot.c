#include "dcr_boost.h"
#include "dcr_path.h"
#include "dcr_setup.h"
#include "error.h"
#include "gl_layer.h"
#include "imports.h"
#include "jni.h"
#include "port_config.h"
#include "rt_applet.h"
#include "rt_boot.h"
#include "rt_window.h"
#include "so_util.h"
#include "util.h"
#include "watchdog.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#define GR "com/gameloft/android/GAND/GloftF5HP/ML/GameRenderer"
/* the game's own screen: the Switch scales this window to the display */
#define F5_W 1024
#define F5_H 600

static so_module game;
static EGLDisplay display;
static EGLConfig egl_config;
static EGLSurface surface;
static EGLContext context;
static unsigned boot_frames;
int fast5_prepare_so(char *out, size_t cap);
int fast5_fix_swp(so_module *m);
void f5_input_init(void *env, void *cls, void *kd, void *ku, void *tp, void *tm, void *tr, void *ac);
void f5_input_poll(void);
void f5_audio_pause(int on);

EGLDisplay b_eglGetDisplay(void *);
EGLBoolean b_eglInitialize(EGLDisplay, EGLint *, EGLint *);
EGLBoolean b_eglChooseConfig(EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *);
EGLContext b_eglCreateContext(EGLDisplay, EGLConfig, EGLContext, const EGLint *);
EGLSurface b_eglCreateWindowSurface(EGLDisplay, EGLConfig, void *, const EGLint *);
EGLBoolean b_eglMakeCurrent(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
EGLBoolean b_eglSwapBuffers(EGLDisplay, EGLSurface);
EGLBoolean b_eglSwapInterval(EGLDisplay, EGLint);

uint64_t dcr_boot_frames(void) { return boot_frames; }

static void *need(const char *name) {
  void *p = (void *)so_try_find_addr_rx(&game, name);
  if (!p)
    fatal_error("Required native export missing: %s", name);
  return p;
}

int port_load(const char *apk) {
  dcr_setup_from_apk(apk);
  log_console_progress("Loading game engine...", 0);
  char path[768];
  if (fast5_prepare_so(path, sizeof path) != 0)
    fatal_error("Could not find libFast5.so (ELF32 ARM) in the game folder.");
  int rc = so_load(&game, path, NULL, PORT_SO_REGION_BYTES);
  if (rc < 0)
    fatal_error("Could not load libFast5.so (ELF32 ARM required), error %d.\n%s", rc, path);
  so_relocate(&game);
  int missing = so_resolve(&game, dcr_imports, dcr_imports_count, 1);
  if (missing)
    fatal_error("libFast5.so has %d unresolved imports. See debug.log.", missing);
  so_fix_kuser_helpers(&game);
  fast5_fix_swp(&game);
  so_finalize(&game);
  so_flush_caches(&game);
  debugPrintf("[elf] libFast5.so mapped at %p, %lu bytes; imports resolved\n", game.load_virtbase,
              (unsigned long)game.load_size);
  return 0;
}

static void egl_start(void) {
  dcr_window_set_size(F5_W, F5_H);
  display = b_eglGetDisplay(NULL);
  EGLint maj, min;
  if (!b_eglInitialize(display, &maj, &min))
    fatal_error("EGL initialization failed.");
  EGLint attrs[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                    EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8, EGL_NONE};
  EGLint num = 0;
  if (!b_eglChooseConfig(display, attrs, &egl_config, 1, &num) || !num) {
    attrs[13] = 16;
    attrs[15] = 0;
    if (!b_eglChooseConfig(display, attrs, &egl_config, 1, &num) || !num)
      fatal_error("No GLES2 window configuration.");
  }
  EGLint ca[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  context = b_eglCreateContext(display, egl_config, EGL_NO_CONTEXT, ca);
  surface = b_eglCreateWindowSurface(display, egl_config, NULL, NULL);
  if (!context || !surface || !b_eglMakeCurrent(display, surface, surface, context))
    fatal_error("Could not create/make current the GLES2 window.");
  b_eglSwapInterval(display, 1);
  debugPrintf("[egl] GLES2 1280x720 ready\n");
}

void port_focus_lost(void) { dcr_boost_idle(); f5_audio_pause(1); }
void port_focus_gained(void) { f5_audio_pause(0); }

void port_run(void) {
  so_execute_init_array(&game);
  so_free_temp(&game);
  egl_start();
  jni_init();

  int (*onload)(void *, void *) = need("JNI_OnLoad");
  int v = onload(g_jni_vm, NULL);
  debugPrintf("[jni] JNI_OnLoad 0x%x\n", v);
  if (v < 0)
    fatal_error("JNI_OnLoad failed (%d).", v);

  void (*render_fn)(void *, void *) = NULL;
  {
    typedef void (*fn_v)(void *, void *);
    typedef void (*fn_ii)(void *, void *, int, int);
    typedef void (*fn_iii)(void *, void *, int, int, int);
#define PKG "Java_com_gameloft_android_GAND_GloftF5HP_ML_"
#define PK "com/gameloft/android/GAND/GloftF5HP/ML/"
    /* GLGame.nativeInit is static (dex: invoke-static GLGame;->nativeInit()V): the receiver is the class */
    ((fn_v)need(PKG "GLUtils_Device_nativeInit"))(g_jni_env, jni_class(PK "GLUtils/Device")->obj);
    ((fn_v)need(PKG "GLResLoader_nativeInit"))(g_jni_env, jni_class(PK "GLResLoader")->obj);
    ((fn_v)need(PKG "GLGame_nativeInit"))(g_jni_env, jni_class(PK "GLGame")->obj);
    ((fn_iii)need(PKG "GameRenderer_nativeInit"))(g_jni_env, jni_class(GR)->obj, F5_W, F5_H, 0);
    ((fn_ii)need(PKG "GameRenderer_nativeResize"))(g_jni_env, jni_class(GR)->obj, F5_W, F5_H);
    render_fn = (fn_v)need(PKG "GameRenderer_nativeRender");
  }
  debugPrintf("[boot] milestone 2: native init done\n");
  f5_input_init(g_jni_env, jni_class(PK "GLGame")->obj, need(PKG "GLGame_nativeSetOnKeyDown"),
                need(PKG "GLGame_nativeSetOnKeyUp"), need(PKG "GLGame_nativeTouchPressed"),
                need(PKG "GLGame_nativeTouchMoved"), need(PKG "GLGame_nativeTouchReleased"),
                need(PKG "GLGame_nativeAccelerometer"));
  dcr_watchdog_start();
  debugPrintf("[log] RAM ring mode on: lines reach the SD every 5 s\n");
  log_set_quiet(1);
  int first = 1;
  while (!rt_exit_requested() && appletMainLoop()) {
    rt_applet_poll();
    if (!rt_focused()) {
      dcr_boost_idle();
      svcSleepThread(50000000);
      continue;
    }
    f5_input_poll();
    dcr_boost_frame_begin();
    glEnable(0x8037); /* GL_POLYGON_OFFSET_FILL */
    glPolygonOffset(0.0f, 1.0f);
    uint64_t t0 = armTicksToNs(armGetSystemTick());
    render_fn(g_jni_env, jni_class(GR)->obj);
    uint64_t t1 = armTicksToNs(armGetSystemTick());
    if ((boot_frames % 120) == 0) {
      unsigned char px[4] = {0};
      glReadPixels(640, 360, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
      debugPrintf("[diag] frame %u center px = %u %u %u %u\n", boot_frames, px[0], px[1], px[2], px[3]);
    }
    glDisable(0x8037);
    boot_frames++;
    dcr_boost_frame_end((uint64_t)boot_frames);
    uint64_t t2 = armTicksToNs(armGetSystemTick());
    int swapped = b_eglSwapBuffers(display, surface);
    uint64_t t3 = armTicksToNs(armGetSystemTick());
    if ((boot_frames % 30) == 0) debugPrintf("[perf] frame %u render %llu ms swap %llu ms\n", boot_frames, (unsigned long long)((t1 - t0) / 1000000), (unsigned long long)((t3 - t2) / 1000000));
    if (!swapped)
      fatal_error("EGL presentation failed; see debug.log.");
    if (first) {
      first = 0;
      dcr_boost_first_picture();
      dcr_boost_launch_end();
    }
  }
  dcr_boost_idle();
  dcr_boost_launch_end();
  rt_applet_stop();
  b_eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(display, context);
  eglDestroySurface(display, surface);
  eglTerminate(display);
  debugPrintf("[boot] clean shutdown\n");
}