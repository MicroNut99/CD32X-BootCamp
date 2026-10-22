# Licences

| Part | Licence |
|---|---|
| `stack/subcpu/`, `hello/src/crt0.s`, `hello/src/32x.h`, `hello/src-md/` | Chilly Willy's (Joseph Fenton's) Sega CD / 32X support code: MIT. See `stack/subcpu/LICENSE.md`. The CD32X additions (the command set, the listener routines, the RAM cart code) come from Kobo Deluxe CD32X and OpenLara CD32X by MicroNut99, under the same terms. |
| `stack/sh2/` (CDLOADER) | MicroNut99's SH2 loader, written to boot Doom 32X: Resurrection from CD and then used by OpenLara CD32X: self-contained, with its own start-up (`cdl_crt0.s`) and linker script. MIT, as the rest of the stack. |
| `hello/src/font8x8.h` | Rasterised from DejaVu Sans Mono Bold. DejaVu fonts licence (Bitstream Vera derived, free to use and redistribute): https://dejavu-fonts.github.io/License.html |
| Everything else written for the BootCamp (`hello/`, `tools/`, `build.sh`, `docs/`) | MIT, below |

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
