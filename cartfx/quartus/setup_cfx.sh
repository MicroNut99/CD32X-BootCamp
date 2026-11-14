#!/bin/bash
# setup_cfx.sh - lays the three cartfx cores into a checkout of krikzz's mega-ed-pub, ready for Quartus
#
#   bash setup_cfx.sh <work>          (<work>/mega-ed-pub/fpga is krikzz's tree; on the owner's PC: fpga_work)
#
# For each game (hur = Hurrican, oj = Jazz, stx = SuperTux):
#   mapper/map_cfx_<g>/   hub.sv (hub_<g>.sv: krikzz's with one line changed), map_ssf.sv (krikzz's, copied unchanged
#                         from mapper/map_ssf/), cfx_core.sv and map_cfx.sv (../rtl)
#   fpga_pro/map_cfx_<g>/ krikzz's map_ssf project (mega-pro.qpf, topcfg.sv, the .qdf) with its .qsf's two mapper lines
#                         replaced by the four files above
# and build_cfx.bat beside <work>'s mega-ed-pub (double-click it: the three .rbf files in <work>\cfx_out\<g>\).
# Only creates and overwrites (the owner's Cowork shell cannot delete). Prints the md5s to check against.
set -e
W=${1:?usage: setup_cfx.sh <work>}
K=$(cd "$(dirname "$0")" && pwd)
F=$W/mega-ed-pub/fpga
[ -f "$F/mapper/map_ssf/map_ssf.sv" ] || { echo "no krikzz tree at $F"; exit 1; }
for g in hur oj stx; do
    m=$F/mapper/map_cfx_$g; p=$F/fpga_pro/map_cfx_$g
    mkdir -p "$m" "$p"
    cp "$K/hub_$g.sv" "$m/hub.sv"
    cp "$F/mapper/map_ssf/map_ssf.sv" "$m/map_ssf.sv"
    cp "$K/../rtl/cfx_core.sv" "$K/../rtl/map_cfx.sv" "$m/"
    cp "$F/fpga_pro/map_ssf/mega-pro.qpf" "$F/fpga_pro/map_ssf/topcfg.sv" "$F/fpga_pro/map_ssf/mega-pro_assignment_defaults.qdf" "$p/"
    # the .qsf: krikzz's, the hub and map_ssf lines pointing at map_cfx_<g>, the engine's two files after them (CRLF and
    # the file's end kept as they are)
    L="set_global_assignment -name SYSTEMVERILOG_FILE ../../mapper"
    sed -e "s#^$L/map_ssf/hub.sv\(\r\?\)\$#$L/map_cfx_$g/hub.sv\1#" \
        -e "s#^$L/map_ssf/map_ssf.sv\(\r\?\)\$#$L/map_cfx_$g/map_ssf.sv\1\n$L/map_cfx_$g/cfx_core.sv\1\n$L/map_cfx_$g/map_cfx.sv\1#" \
        "$F/fpga_pro/map_ssf/mega-pro.qsf" > "$p/mega-pro.qsf"
    n=$(grep -c "mapper/map_cfx_$g/" "$p/mega-pro.qsf")
    [ "$n" = 4 ] || { echo "map_cfx_$g: the .qsf has $n of the 4 file lines (krikzz's .qsf changed?)"; exit 1; }
done
cp "$K/build_cfx.bat" "$W/build_cfx.bat"
md5sum "$F/mapper/map_ssf/map_ssf.sv" "$F"/mapper/map_cfx_*/map_ssf.sv "$F"/mapper/map_cfx_*/cfx_core.sv "$F"/mapper/map_cfx_*/map_cfx.sv
echo "ready: double-click $W/build_cfx.bat"
