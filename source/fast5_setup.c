#include "dcr_setup.h"
#include "port_config.h"

char fast5_dso_handle;

static const char *const libs[] = {"libFast5.so"};

const RtSetupPlan port_setup_plan = {
    .libs = libs,
    .nlibs = 1,
    .libs_p0 = 0,
    .libs_p1 = 900,
    .classes_p0 = 900,
    .classes_p1 = 1000,
};