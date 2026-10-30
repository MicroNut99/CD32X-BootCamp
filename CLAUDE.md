# Read this first (for an AI working in this repository)

This kit is the CD32X boot stack. The reference material lives in a second repository,
[MicroNut99/docs](https://github.com/MicroNut99/docs), checked out here as the `manuals/` submodule:

```
git submodule update --init          # fetches manuals/ (run once after cloning)
```

## Reading rules

1. **Two documents must be read in full before any work on this stack. Not skimmed, not sampled,
   not summarised from their headings: every line.**
   - `manuals/CD32X_COMMAND_LEDGER_ADDITIONS.md`: the master list of every Sub-CPU command, who owns
     each save piece (§5), the listener routines.
   - `manuals/CD32X_COMMAND_PROGRAMMING_GUIDE.md`: how a command is added, step by step.

   Read them with the Read tool from top to bottom, in pieces if they're long, until the end of the file.
   If a tool returns only part of a file, keep reading from where it stopped.

2. **All the manuals must be read**, including the PDFs (`32x_hardware_manual.pdf`,
   `Mega-CD_Hardware_Manual.pdf`, the Kobo PDFs, the addenda), before changing anything they describe.
   [docs/08_RESEARCH_INDEX.md](docs/08_RESEARCH_INDEX.md) says what each one is.

3. **The boot stack (`stack/`) is off limits** unless the owner asks for a change to it by name.

4. **The emulator to test with is the owner's ares build,
   [MicroNut99/ares_android](https://github.com/MicroNut99/ares_android).** It is made for CD32X,
   32XCD and EDCD (EverDrive PRO) boot games, and runs regular 32X games too. Use it, from your own
   container, for every test run: how to build its headless runner, get the disc in, run it and read
   the result is in [docs/13](docs/13_ARES_FOR_AI.md); what it models and its limits are in
   [docs/11](docs/11_ARES_16MB_RAM_CART.md) and [`ares/`](ares/). No other emulator counts for this
   stack. And the emulator follows the hardware: a green run in ares is never a pass for the console.

5. Never commit game data, discs, BIOS files or music to this or any public repository.

6. **The Evolution files are required reading, in full and in order, before anything else in this
   repository: [Evolution1.md](Evolution1.md), then [Evolution2.md](Evolution2.md).** Each is the
   record of what a previous AI found after reading the whole kit. Evolution1 (29 Oct 2026, commit
   `9e9ca98`): what the stack is, how it boots, the contract for a game, the EverDrive PRO findings
   and the process rules, in one page. Evolution2 (29 Oct 2026, commit `cdab01b`): the mechanics as
   the source has them, the EDCD launcher step by step, the probes, the `sdcard/` kit, what Sega's and
   Hitachi's manuals confirm or contradict, and a list of places where the documents and the code
   disagree. Start from them, not from the folder listing. A further full reading adds
   `Evolution<n+1>.md` and this rule names it; it never rewrites an earlier one.

## Reading order (a new AI learns this stack fastest this way)

0. [Evolution1.md](Evolution1.md), then [Evolution2.md](Evolution2.md): the whole kit as last read,
   and the list of what to distrust in the documents until it is fixed.
1. [README.md](README.md), then [docs/01_HOW_IT_BOOTS.md](docs/01_HOW_IT_BOOTS.md): what runs where,
   power-on to `main()`.
2. The two must-read documents, in full: `manuals/CD32X_COMMAND_LEDGER_ADDITIONS.md`, then
   `manuals/CD32X_COMMAND_PROGRAMMING_GUIDE.md`.
3. The Kobo PDFs in `manuals/`: `KOBO-CD32X_BOOT_PROCESS.pdf`, then `KOBO-Sega 32XCD Boot Sequence
   and Sub-CPU Command Ledger.pdf` (the root of every disc game here).
4. [docs/03_COMMANDS.md](docs/03_COMMANDS.md), [docs/04_MEMORY_MAPS.md](docs/04_MEMORY_MAPS.md),
   [docs/06_HARDWARE_RULES.md](docs/06_HARDWARE_RULES.md): this kit's commands, maps and the rules
   learned on the console.
5. Sega's manuals in `manuals/`: `32x_hardware_manual.pdf`, `Mega-CD_Hardware_Manual.pdf`; then the
   addenda and the rest of `manuals/` ([docs/08](docs/08_RESEARCH_INDEX.md) says what each is).
6. Testing: [docs/11](docs/11_ARES_16MB_RAM_CART.md), [docs/13](docs/13_ARES_FOR_AI.md), then build
   ares_android's headless runner and run `hello/`.
7. Only then [docs/02_PORTING_YOUR_GAME.md](docs/02_PORTING_YOUR_GAME.md) and the game you're here for.

If `manuals/` is empty, the submodule hasn't been fetched: run the command above. Don't work from
memory of these documents, and don't substitute the web for them.
