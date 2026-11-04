/* GL wrapper hook of the android32 runtime: diagnostics (PVRTC first), then the shader source dump. */
#include <stdint.h>
uintptr_t fast5_diag_wrap(const char *name, uintptr_t real);
uintptr_t fast5_shdump_wrap(const char *name, uintptr_t real);
uintptr_t port_gl_wrap(const char *name, uintptr_t real) {
  uintptr_t d = fast5_diag_wrap(name, real);
  uintptr_t s = fast5_shdump_wrap(name, d ? d : real);
  return s ? s : d;
}
