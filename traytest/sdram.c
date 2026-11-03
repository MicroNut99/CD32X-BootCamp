/* traytest/sdram.c - the start of the tray test's SH2 program when it runs from SDRAM (T3: no cart)
 *
 * T3 (the owner, 2 Nov 2026: "we cannot have the test tied to a MedPro or 16MB cart"): the test program is
 * not a cartridge ROM loaded into a RAM cart.  It is built like CDLOADER, with the stack's own CD start-up
 * (stack/sh2/cdl_crt0.s: the CD boot table the 32X boot ROM reads) and linker script (stack/sh2/cdloader.ld),
 * and it takes CDLOADER's place in the Sub-CPU program (ROMS/CDLOADER.BIN).  The 32X boot ROM copies it
 * from the frame buffer into SDRAM (0x06000000) and starts it there, as it starts CDLOADER (and as Kobo
 * Deluxe, the root of this stack, ran: docs/01, "Two other ways").  No cartridge is read or written.
 *
 * main() below is CDLOADER's own start-up (stack/sh2/cdloader_sh2.c, main(), the lines before "1. the ROM
 * image into the cart"), line for line: the COMM10 handshake with the Sub-CPU (0x0AAA / 0x0BBB) and the
 * frame buffer taken for the SH2.  Then the test runs: main.c's main(), compiled as tray_main()
 * (build_traytest.sh: -Dmain=tray_main).
 * cdl_crt0.s jumps to the label _main, which on SH-ELF is C's main() (C names get a leading underscore):
 * a C function called _main would be __main and never run.  (The first T3 build had that mistake: the
 * test ran without the handshake, and the Sub-CPU stopped after its 3 s wait, "SH2 NEVER SIGNALLED READY".)
 */
#include <stddef.h>
#include <stdint.h>

#define MARS_SYS_INTMSK  (*(volatile uint16_t *)0x20004000)
#define MARS_SYS_COMM0   (*(volatile uint16_t *)0x20004020)
#define SC_COMM10        (*(volatile uint16_t *)0x2000402A)
#define SC_SYS_ADAPTER_B (*(volatile uint8_t  *)0x20004000)
#define SC_CMD_CLR       (*(volatile uint16_t *)0x2000401A)

/* in the disc image: build_traytest.sh checks for it (and that CDLOADER's marker is not there) */
__attribute__((used)) const char sh2_build_marker[] = "SH2-TRAYTEST-SDRAM";

/* what cdl_crt0.s needs besides main.c's pri_vbi_handler, sec_dma1_handler, secondary and the counters */
void pri_cmd_handler(void) { SC_CMD_CLR = 0; }
void sec_cmd_handler(void) { SC_CMD_CLR = 0; }

/* no C library (-nostdlib, as CDLOADER): our own, volatile so GCC cannot turn them into calls to themselves */
void *memset(void *d, int c, size_t n) {
    volatile uint8_t *p = (volatile uint8_t *)d;
    while (n--) *p++ = (uint8_t)c;
    return d;
}
void *memcpy(void *d, const void *s, size_t n) {
    volatile uint8_t *p = (volatile uint8_t *)d;
    const volatile uint8_t *q = (const volatile uint8_t *)s;
    while (n--) *p++ = *q++;
    return d;
}
void *memmove(void *d, const void *s, size_t n) { return memcpy(d, s, n); }

int tray_main(void);                         /* main.c's main() */

int main(void)
{
    /* CDLOADER's start-up (cdloader_sh2.c), unchanged */
    MARS_SYS_COMM0 = 0;
    SC_COMM10 = 0x0006;
    MARS_SYS_INTMSK |= 0x0002;
    SC_COMM10 = 0x0007;
    __asm__ __volatile__("ldc %0,sr" : : "r"(0) : "memory");
    SC_COMM10 = 0x0AAA;
    while (SC_COMM10 != 0x0BBB) { }
    SC_SYS_ADAPTER_B = 0x80;                     /* take_fb(): the frame buffer to the SH2 */
    while ((MARS_SYS_INTMSK & 0x8000) == 0) { }
    tray_main();
    while (1) { }
    return 0;
}
