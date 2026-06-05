#include "dos_it8888.h"

u8 it8_in8(u16 port) {
#if defined(__WATCOMC__)
  return inp(port);
#else
  return 0xFF;
#endif
}

u16 it8_in16(u16 port) {
#if defined(__WATCOMC__)
  return inpw(port);
#else
  (void)port;
  return 0xFFFFu;
#endif
}

u32 it8_in32(u16 port) {
#if defined(__WATCOMC__)
  u32 v;
  __asm {
        mov dx, port
        in  eax, dx
        mov dword ptr v, eax
  }
  return v;
#else
  (void)port;
  return 0xFFFFFFFFul;
#endif
}

void it8_out8(u16 port, u8 v) {
#if defined(__WATCOMC__)
  outp(port, v);
#else
  (void)port;
  (void)v;
#endif
}

void it8_out16(u16 port, u16 v) {
#if defined(__WATCOMC__)
  outpw(port, v);
#else
  (void)port;
  (void)v;
#endif
}

void it8_out32(u16 port, u32 v) {
#if defined(__WATCOMC__)
  __asm {
        mov dx, port
        mov eax, dword ptr v
        out dx, eax
  }
#else
  (void)port;
  (void)v;
#endif
}
