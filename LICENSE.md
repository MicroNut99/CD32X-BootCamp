# Licences

| Part | Licence |
|---|---|
| `stack/subcpu/`, `hello/src/crt0.s`, `hello/src/32x.h`, `hello/src-md/` | Chilly Willy's (Joseph Fenton's) Sega CD / 32X support code: MIT. See `stack/subcpu/LICENSE.md`. The CD32X additions (the command set, the listener routines, the RAM cart code) come from Kobo Deluxe CD32X and OpenLara CD32X by MicroNut99, under the same terms. |
| `stack/sh2/` (CDLOADER) | `cdl_crt0.s`: Chilly Willy's SH2 start-up (MIT, `stack/subcpu/LICENSE.md`) with a CD boot table added. `cdloader_sh2.c`, `cdloader.ld`, `Makefile.cdloader`: MicroNut99's SH2 loader, written to boot Doom 32X: Resurrection from CD and then used by OpenLara CD32X; MIT. |
| `hello/src/font8x8.h`, `edtest/src/font8x8.h` | Rasterised from DejaVu Sans Mono Bold. DejaVu fonts licence (Bitstream Vera derived, free to use and redistribute): https://dejavu-fonts.github.io/License.html |
| `edtest/src-md/`, `edtest/src/crt0.s`, `edtest/src/32x.h` | Chilly Willy's 32X cartridge start-up and Mega CD mode 1 code (MIT, `stack/subcpu/LICENSE.md`), with the test's commands added |
| `ares/everdrive-pro-ares-v148.patch` | A change to the ares emulator: under ares's ISC licence (https://github.com/ares-emulator/ares) |
| Everything else written for the BootCamp (`hello/`, `edtest/`, `edboot/`, `tools/`, `build.sh`, `docs/`) | MIT, below |

```
MIT License

Copyright (c) 2026 MicroNut99

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial
portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

Not included and not covered: the Sega CD BIOS, the boot block (`US_BOOT.BIN`, from the toolchain), any
game's data. Sega, Mega Drive, Genesis, Sega CD and 32X are Sega's trademarks; this project is not
affiliated with Sega.
