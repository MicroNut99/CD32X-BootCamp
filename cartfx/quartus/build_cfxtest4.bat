@echo off
rem CFXTEST 4 (a wrong word read again: the console's read or the cart's memory), built in WSL with the owner's
rem toolchain; then the SD card's folder cartfx_work\sd4\ with it and v3's Hurrican core (sd3's mega-pro.rbf, unchanged).
set C=S:\KoboPort\sega-toolchain-12.1\sega\cartfx_work
echo building CFXTEST.32X in WSL ...
wsl -e bash -lc "cd /mnt/s/KoboPort/sega-toolchain-12.1/sega/cartfx_work/cartfx/test && bash build.sh > ../../cfxtest_build.log 2>&1; echo exit $? >> ../../cfxtest_build.log"
if not exist "%C%\sd4" mkdir "%C%\sd4"
copy /y "%C%\cartfx\test\CFXTEST.32X" "%C%\sd4\CFXTEST.32X" > nul
copy /y "%C%\sd3\mega-pro.rbf" "%C%\sd4\mega-pro.rbf" > nul
echo done > "%C%\cfxtest4_done.txt"
