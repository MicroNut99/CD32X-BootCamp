# 08 – Research index

## The repositories this kit was built from (state compared in [07](07_METHODS_COMPARED.md))

| Repository | Commit | What to read there |
|---|---|---|
| [MicroNut99/OpenLaraCD32X-REV](https://github.com/MicroNut99/OpenLaraCD32X-REV) | v1.7 | `OpenLaraCD32X/subcpu`, `sh2/` (this kit's stack); `docs/OPENLARA_CD32X_RELEASE.md` (every change, the console rules) |
| [MicroNut99/OpenLaraCD32X](https://github.com/MicroNut99/OpenLaraCD32X) | `425115f` | The full original stack: every Sub-CPU file, `build.sh` |
| [MicroNut99/OpenJazzCD32X-REV](https://github.com/MicroNut99/OpenJazzCD32X-REV) | `79b9cf0` (R11) | `cd32x/subcpu/main.c` (cached directory, streaming, CMD 67–71); `docs/GENESIS_PLANE.md`; `port/pack.h` (cart layout) |
| [MicroNut99/HurricanCD32X](https://github.com/MicroNut99/HurricanCD32X) | `b70770a` | The slave worker (`port/plat_mars.c`), the PC host tests, music from IT modules |
| [MicroNut99/SuperTuxCD32X_REV](https://github.com/MicroNut99/SuperTuxCD32X_REV) | `9f3fde6` | `tools/check_boot.py` + `docs/BOOT_FILES.md5`; CMD 62–67 (Genesis VDP); 16 MB preload |

## The documents: [MicroNut99/docs](https://github.com/MicroNut99/docs)

| File | What it is |
|---|---|
| `KOBO-CD32X_BOOT_PROCESS.pdf` | How Kobo Deluxe boots and runs: the root of every disc game here |
| `KOBO-Sega 32XCD Boot Sequence and Sub-CPU Command Ledger.pdf` | The boot sequence (from Night Trap) and the original command ledger |
| `CD32X_COMMAND_LEDGER_ADDITIONS.md` | **The master list**: every command added since, the save-piece owners (§5), the listener routines |
| `CD32X_COMMAND_PROGRAMMING_GUIDE.md` | How to add a command, step by step |
| `How the current CD32X RAM Cart Games work.pdf` | Overview of the RAM-cart games |
| `CD32X_ADDENDUM_OPENLARA.pdf` | OpenLara: CDLOADER, the ROM in the cart |
| `CD32X_ADDENDUM_OPENJAZZ2.pdf`, `JazzBuildNotes_10062026_05.pdf` | OpenJazz: packs, saves, the build |
| `CD32X_ADDENDUM_OPENTYRIAN.pdf` | The other architecture: a 32X cartridge with the Sega CD attached |
| `CD32X_STREAM_READ.md` | The CD read-ahead (`STREAM`) |
| `CD32X_CART_PAGING_PROPOSAL.md`, `CD32X_PAGED_CART_STACK_PROPOSAL.md`, `CD32X_SRAM_PAGING_PROPOSAL.md`, `CD32X_PAGING_SONICCD.md` | The 8 / 16 MB paged cart |
| `DIAG_TEXT_HOWTO.md` | Turning the boot diagnostics on and off |
| `OPENLARA_CD32X_RELEASE.md` | OpenLara CD32X release notes |
| `32x_hardware_manual.pdf`, `Mega-CD_Hardware_Manual.pdf` | Sega's hardware manuals |
| `sega-vr-disasm/` | Virtua Racing disassembly (32X reference) |
| `releases.txt` | Links to the released discs and ROMs |

## Other MicroNut99 repositories

| Repository | What |
|---|---|
| [kobo32x](https://github.com/MicroNut99/kobo32x), [kobo32x_hw](https://github.com/MicroNut99/kobo32x_hw), [kobo32x_crt](https://github.com/MicroNut99/kobo32x_crt) | Kobo Deluxe 32X / CD32X: the original stack |
| [OpenJazzCD32X](https://github.com/MicroNut99/OpenJazzCD32X), [SuperTuxCD32X](https://github.com/MicroNut99/SuperTuxCD32X) | The earlier versions of the REV repositories |
| [OpenTyrianCD32X](https://github.com/MicroNut99/OpenTyrianCD32X) | The cartridge + CD architecture |
| [MD-32X-RAM-Cart-32Bit](https://github.com/MicroNut99/MD-32X-RAM-Cart-32Bit) | The RAM cart hardware |
| [ares_android](https://github.com/MicroNut99/ares_android) | **The emulator for this stack:** ares v148 with the 4 / 16 MB RAM carts and the EverDrive PRO, plus `ares-headless` for testing from a container. Made for CD32X, 32XCD and EDCD boot games; regular 32X games too. How to use it: [docs/13](13_ARES_FOR_AI.md) |
| [Sonic32XPlus1](https://github.com/MicroNut99/Sonic32XPlus1), [Sonic32XPlus1-RealHardware](https://github.com/MicroNut99/Sonic32XPlus1-RealHardware), [BlakeStone32X](https://github.com/MicroNut99/BlakeStone32X), [AnotherWorld32X](https://github.com/MicroNut99/AnotherWorld32X), [VitualRacing32X](https://github.com/MicroNut99/VitualRacing32X) | 32X cartridge projects (no CD) |

## Outside

- RAM cart: https://consolemods.org/wiki/Genesis:MD_32X_RAM_Cart
- Chilly Willy's Sega CD / 32X toolchain and examples: the base of all of the above.
- Doom 32X: Resurrection (Victor Luchits and team): the game CDLOADER and CMD 60 were first written to boot.
