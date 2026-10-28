#include "jni.h"
#include "port_config.h"
#include <stddef.h>

#define GLGAME "com/gameloft/android/GAND/GloftF5HP/ML/GLGame"

const JMethodDef jni_method_defs[] = {{NULL, NULL, NULL, NULL}};

const JFieldDef jni_field_defs[] = {
    {"android/os/Build$VERSION", "SDK_INT", NULL, 19, NULL},
    {"android/content/Context", "WINDOW_SERVICE", NULL, 0, "window"},
    {NULL, NULL, NULL, 0, NULL}};

const char *const jni_class_supers[][2] = {{GLGAME, "android/app/Activity"}, {NULL, NULL}};

const char *const jni_missing_classes[] = {NULL};