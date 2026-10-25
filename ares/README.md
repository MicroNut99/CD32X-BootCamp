# ares: the Mega EverDrive PRO as a 16 MB cart

`everdrive-pro-ares-v148.patch` adds the Mega EverDrive PRO's extended SSF mapper to the ares emulator, v148:
16 MB of RAM in 32 banks of 512 KB, writable with CTRL0's W bit, and the C bit that releases CART_IN.

```
git clone --branch v148 https://github.com/ares-emulator/ares.git
cd ares
git apply /path/to/CD32X-BootCamp/ares/everdrive-pro-ares-v148.patch
```

Then build ares as usual. A ROM with `SEGA SSF` in its header (`edtest/EDTEST_W.32X`, Load → Mega 32X) now runs
on the EverDrive board: `edtest` shows `ALL TESTED BANKS OK`, as on the console.

Using it as the RAM cart of a CD32X disc also needs a frontend that puts a cart in the slot while the disc
boots: [docs/11](../docs/11_ARES_16MB_RAM_CART.md), section 5. Everything about the board, what is proven and
what is a choice, and the tests: docs/11.

Licence: the patch changes ares and is under ares's ISC licence (https://github.com/ares-emulator/ares).
