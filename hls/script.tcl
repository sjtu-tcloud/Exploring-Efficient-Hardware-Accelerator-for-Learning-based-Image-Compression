############################################################
## This file is generated automatically by Vivado HLS.
## Please DO NOT edit it.
## Copyright (C) 1986-2019 Xilinx, Inc. All Rights Reserved.
############################################################
open_project ic_acc_i16c_hls1902
set_top IC_codec
add_files acc.cpp
add_files acc_new_reorg.h
open_solution "solution1"
set_part {xczu9eg-ffvb1156-2-e}
create_clock -period 3.2 -name default
config_sdx -optimization_level none -target none
config_export -vivado_optimization_level 2
#source "./ic_acc_i16c_hls1902/solution1/directives.tcl"
#csim_design
csynth_design
#cosim_design
export_design -rtl verilog -format ip_catalog -version "0.0.0"
