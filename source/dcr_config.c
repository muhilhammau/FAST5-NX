#include <stddef.h>
#include "config.h"
#include "rt_cfg.h"
float fast5_deadzone = 0.18f;
int fast5_swap_ab;
static const CfgOpt opts[] = {
    {"display", "resolution", "720", "1280 x 720 landscape.", CFG_CHOICE, "720", NULL},
    CFG_ROW_BOOST(CFG_HELP_BOOST, NULL),
    CFG_ROW_SWAP_AB("Swap the gamepad's bottom and right face buttons.", &fast5_swap_ab),
    {"controls", "deadzone", "0.18", "Analog stick deadzone.", CFG_FLOAT, NULL, &fast5_deadzone,
     0.05f, 0.4f},
    CFG_ROW_GL_SELFTEST(NULL),
    CFG_ROW_BOOT_LOG(CFG_HELP_BOOT_LOG, NULL),
    CFG_ROW_LOG_JNI(CFG_HELP_LOG_JNI, NULL)};
static const CfgTable table = {.opts = opts, .nopts = CFG_COUNT(opts), .version = 1};
void dcr_config_load(void) { rt_config_load(&table); }