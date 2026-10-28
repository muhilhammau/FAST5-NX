#include <elf.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dcr_setup.h"
#include "so_util.h"
#include "util.h"

#define FAST5_SWP_POOL_BYTES 4096u
#define STUB_WORDS 10
#ifndef PT_GNU_STACK
#define PT_GNU_STACK 0x6474e551
#endif

uint32_t fast5_swp_pool_off;

static int is_swp(uint32_t w) { return (w >> 28) != 0xF && (w & 0x0FF00FF0u) == 0x01000090u; }
static int is_swpb(uint32_t w) { return (w >> 28) != 0xF && (w & 0x0FF00FF0u) == 0x01400090u; }

static uint32_t b_enc(uint32_t cond, const uint32_t *from, const uint32_t *to) {
  int32_t off = (int32_t)(((intptr_t)to - ((intptr_t)from + 8)) >> 2);
  return cond << 28 | 0x0A000000u | ((uint32_t)off & 0x00FFFFFFu);
}

typedef struct {
  uint32_t *p, *end;
  int fixed, bad_regs, no_pool, far;
} SwpPool;

static void fix_swp(uint32_t *site, uint32_t w, SwpPool *P) {
  const uint32_t cond = w >> 28, rn = (w >> 16) & 15, rd = (w >> 12) & 15, rm = w & 15;
  if (rn >= 13 || rd >= 13 || rm >= 13 || rd == rn || rd == rm) {
    P->bad_regs++;
    return;
  }
  if (P->p + STUB_WORDS > P->end) {
    P->no_pool++;
    return;
  }
  uint32_t *s = P->p;
  const intptr_t d1 = (intptr_t)s - ((intptr_t)site + 8);
  const intptr_t d2 = (intptr_t)(site + 1) - ((intptr_t)(s + 9) + 8);
  if (d1 > 0x7FFFFC || d1 < -0x800000 || d2 > 0x7FFFFC || d2 < -0x800000) {
    P->far++;
    return;
  }
  const uint32_t used = 1u << rn | 1u << rd | 1u << rm;
  uint32_t ra = 0, rb = 0;
  int found = 0;
  for (uint32_t r = 0; r <= 12 && found < 2; r++)
    if (!(used >> r & 1)) {
      if (!found++)
        ra = r;
      else
        rb = r;
    }
  s[0] = 0xE92D0000u | 1u << ra | 1u << rb;      /* push {ra, rb} */
  s[1] = 0xE10F0000u | rb << 12;                 /* mrs rb, apsr */
  s[2] = 0xE1900F9Fu | rn << 16 | rd << 12;      /* ldrex rd, [rn] */
  s[3] = 0xE1800F90u | rn << 16 | ra << 12 | rm; /* strex ra, rm, [rn] */
  s[4] = 0xE3500000u | ra << 16;                 /* cmp ra, #0 */
  s[5] = b_enc(0x1, &s[5], &s[2]);               /* bne ldrex */
  s[6] = 0xF57FF05Bu;                            /* dmb ish */
  s[7] = 0xE128F000u | rb;                       /* msr apsr_nzcvq, rb */
  s[8] = 0xE8BD0000u | 1u << ra | 1u << rb;      /* pop {ra, rb} */
  s[9] = b_enc(0xE, &s[9], site + 1);            /* b back */
  *site = b_enc(cond, site, s);
  P->p += STUB_WORDS;
  P->fixed++;
}

int fast5_fix_swp(so_module *m) {
  if (!fast5_swp_pool_off) {
    debugPrintf("[swp] no pool: fast5_prepare_so did not run\n");
    return 0;
  }
  uint32_t *pool = (uint32_t *)((uint8_t *)m->load_base + fast5_swp_pool_off);
  const size_t pool_words = FAST5_SWP_POOL_BYTES / 4;
  SwpPool P = {pool, pool + pool_words, 0, 0, 0, 0};
  uint8_t *base = m->load_base;
  int swpb = 0;
  uint32_t tlo = 0, thi = 0;
  for (int i = 0; i < m->elf_hdr->e_shnum; i++)
    if (!strcmp(m->shstrtab + m->sec_hdr[i].sh_name, ".text")) {
      tlo = m->sec_hdr[i].sh_addr;
      thi = tlo + m->sec_hdr[i].sh_size;
    }
  for (int i = 0; i < m->phnum && thi; i++) {
    const Elf32_Phdr *ph = &m->phdr[i];
    if (ph->p_type != PT_LOAD || !(ph->p_flags & PF_X))
      continue;
    uint32_t lo = ph->p_vaddr, hi = lo + ph->p_filesz;
    if (lo < tlo) lo = tlo;
    if (hi > thi) hi = thi;
    for (uint32_t off = lo; off + 4 <= hi; off += 4) {
      uint32_t *site = (uint32_t *)(base + off);
      const uint32_t w = *site;
      if (is_swpb(w)) {
        swpb++;
        continue;
      }
      if (is_swp(w))
        fix_swp(site, w, &P);
    }
  }
  debugPrintf("[swp] %d SWP -> stubs; %d SWPB left, %d bad regs, %d out of range, %d no pool\n", P.fixed,
              swpb, P.bad_regs, P.far, P.no_pool);
  return P.fixed;
}

int fast5_prepare_so(char *out, size_t cap) {
  char src[768];
  rt_root_path(src, sizeof src, "libFast5.so");
  rt_root_path(out, cap, "swp/libFast5.so");
  size_t len = 0;
  uint8_t *buf = rt_read_whole(src, &len);
  if (!buf)
    return -1;
  int rc = -2;
  Elf32_Ehdr *eh = (Elf32_Ehdr *)buf;
  if (len < sizeof *eh || memcmp(buf, ELFMAG, SELFMAG) || eh->e_phentsize != sizeof(Elf32_Phdr) ||
      (size_t)eh->e_phoff + (size_t)eh->e_phnum * sizeof(Elf32_Phdr) > len)
    goto done;
  Elf32_Phdr *ph = (Elf32_Phdr *)(buf + eh->e_phoff);
  int spare = -1;
  uint32_t end = 0;
  for (int i = 0; i < eh->e_phnum; i++) {
    if (ph[i].p_type == PT_LOAD && ph[i].p_vaddr + ph[i].p_memsz > end)
      end = ph[i].p_vaddr + ph[i].p_memsz;
    if (ph[i].p_type == PT_GNU_STACK)
      spare = i;
  }
  if (spare < 0 || !end) {
    rc = -3;
    goto done;
  }
  fast5_swp_pool_off = (end + 0xFFFu) & ~0xFFFu;
  memset(&ph[spare], 0, sizeof ph[spare]);
  ph[spare].p_type = PT_LOAD;
  ph[spare].p_vaddr = ph[spare].p_paddr = fast5_swp_pool_off;
  ph[spare].p_memsz = FAST5_SWP_POOL_BYTES;
  ph[spare].p_flags = 5u; /* R+X */
  ph[spare].p_align = 0x1000;
  const int wrote = rt_write_if_changed(out, buf, len);
  rc = rt_file_size(out) == (long)len ? 0 : -4;
  debugPrintf("[swp] %s: RX pool at +0x%x (%s)\n", out, (unsigned)fast5_swp_pool_off,
              wrote ? "copy written" : "copy up to date");
done:
  free(buf);
  return rc;
}