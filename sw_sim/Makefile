
MAX_S=4
MAX_K=9
MAX_BETA_LENGTH=128

MAX_Tif=16
MAX_Tof=32
MAX_Pif=16
MAX_Pof=4
MAX_Poy=1
MAX_Pox=4
LANE_NUM=4

MAX_Tr=24
MAX_Tc=24

CFLAGS += -D MAX_Pif=$(MAX_Pif) -D MAX_Pof=$(MAX_Pof) -D MAX_Poy=$(MAX_Poy) -D MAX_Pox=$(MAX_Pox)
CFLAGS += -D MAX_S=$(MAX_S) -D MAX_K=$(MAX_K) -D MAX_Tif=$(MAX_Tif) -D MAX_Tof=$(MAX_Tof)
CFLAGS += -D MAX_Tr=$(MAX_Tr) -D MAX_Tc=$(MAX_Tc) -D MAX_BETA_LENGTH=$(MAX_BETA_LENGTH)
CFLAGS += -D LANE_NUM=$(LANE_NUM)
CFLAGS += -D _DEF_IN_MAKEFILE_

test_as_rsc_f32c_NTP:
	g++ -O3 -w -o test_ana main_ana_chw_rsc_0_NTP.cpp -I . -lm $(CFLAGS)
	g++ -O3 -w -o test_syn main_syn_chw_rsc_0_NTP.cpp -I . -lm $(CFLAGS)
	./test_ana ../abigail-keenan-27293.png
	./test_syn ana_q_out_chw.bin 86 128

gen_reorder_w_sk_f32c:
	g++ -O3 -w -o w_reorder weight_reorder_sk_f32c.cpp -I . -lm
	./w_reorder	$(LANE_NUM) $(MAX_Tif) $(MAX_Tof)

clean:
	rm -rf *.png
	rm -rf test_*
clean_bin:
	rm -rf *.bin
clean_test:
	rm -f test_*
