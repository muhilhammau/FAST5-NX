#include "dcr_setup.h"
#include "port_config.h"

char fast5_dso_handle;

static const char *const libs[] = {"libFast5.so"};

const RtSetupPlan port_setup_plan = {
    .libs = libs,
    .nlibs = 1,
    .libs_what = "Unpacking the game library",
    .apk_requirement = "This port needs the 32-bit ARM (armeabi-v7a) build of Fast Five HD\n"
                       "(com.gameloft.android.GAND.GloftF5HP.ML): use the APK of your own copy.",
    .libs_p0 = 0,
    .libs_p1 = 900,
    .classes_p0 = 900,
    .classes_p1 = 1000,
};