
MAX_S=4
MAX_K=9
MAX_BETA_LENGTH=128

MAX_Tif=16
MAX_Tof=16
MAX_Tr=24
MAX_Tc=24
LANE_NUM=8

MAX_Pif=4
MAX_Pof=16
MAX_Poy=1
MAX_Pox=16

CFLAGS += -D MAX_Pif=$(MAX_Pif) -D MAX_Pof=$(MAX_Pof) -D MAX_Poy=$(MAX_Poy) -D MAX_Pox=$(MAX_Pox)
CFLAGS += -D MAX_S=$(MAX_S) -D MAX_K=$(MAX_K) -D MAX_Tif=$(MAX_Tif) -D MAX_Tof=$(MAX_Tof)
CFLAGS += -D MAX_Tr=$(MAX_Tr) -D MAX_Tc=$(MAX_Tc) -D MAX_BETA_LENGTH=$(MAX_BETA_LENGTH)
CFLAGS += -D LANE_NUM=$(LANE_NUM)
CFLAGS += -D _DEF_IN_MAKEFILE_

gen_all: gen_reorder_w_sk_f32c gen_iofm_sk_i16c gen_reorder_w_sk_i16c_scale gen_interQ_sk_i16c gen_w_sr_cb 

test_as_rsc_i16c_NTP:
	g++ -O3 -w -o test_ana main_ana_chw_rsc_0_4_NTP.cpp -I . -lm $(CFLAGS)
	g++ -O3 -w -o test_syn main_syn_chw_rsc_0_4_NTP.cpp -I . -lm $(CFLAGS)
	./test_ana ../img/abigail-keenan-27293.png
	./test_syn ana_q_out_chw.bin ana_IHW_OHW_set.bin

gen_w_sr_cb:
	g++ -O3 -w -o test_ana gen_w_sr_cb.cpp -I . -lm $(CFLAGS)
	./test_ana

test_interQ_sk_i16c:
	g++ -O3 -w -o test_ana main_ana_chw_rsc_0_4.cpp -I . -lm $(CFLAGS)
	g++ -O3 -w -o test_syn main_syn_chw_rsc_0_4.cpp -I . -lm $(CFLAGS)
	./test_ana ../img/abigail-keenan-27293.png
	./test_syn ana_q_out_chw.bin ana_IHW_OHW_set.bin

gen_interQ_sk_i16c:
	g++ -O3 -w -o test_ana main_ana_chw_rsc_0_3.cpp -I . -lm $(CFLAGS)
	g++ -O3 -w -o test_syn main_syn_chw_rsc_0_3.cpp -I . -lm $(CFLAGS)
	./test_ana ../img/abigail-keenan-27293.png
	./test_syn ana_q_out_chw.bin ana_IHW_OHW_set.bin

test_iofm_reorder_w_sk_i16c:
	g++ -O3 -w -o test_ana main_ana_chw_rsc_0_2.cpp -I . -lm $(CFLAGS)
	g++ -O3 -w -o test_syn main_syn_chw_rsc_0_2.cpp -I . -lm $(CFLAGS)
	./test_ana ../img/abigail-keenan-27293.png
	./test_syn ana_q_out_chw.bin ana_IHW_OHW_set.bin

gen_reorder_w_sk_i16c_scale:
	g++ -O3 -w -o test_reorder weight_reorder_sk_i16c_scale.cpp -I . -lm
	./test_reorder	$(LANE_NUM) $(MAX_Tif) $(MAX_Tof)

gen_iofm_sk_i16c:
	g++ -O3 -w -o test_ana main_ana_chw_rsc_0_1.cpp -I . -lm $(CFLAGS)
	g++ -O3 -w -o test_syn main_syn_chw_rsc_0_1.cpp -I . -lm $(CFLAGS)
	./test_ana ../img/abigail-keenan-27293.png
	./test_syn ana_q_out_chw.bin ana_IHW_OHW_set.bin

#f32c start######################################################################################
test_as_rsc_f32c_NTP:
	g++ -O3 -w -o test_ana main_ana_chw_rsc_0_NTP.cpp -I . -lm $(CFLAGS)
	g++ -O3 -w -o test_syn main_syn_chw_rsc_0_NTP.cpp -I . -lm $(CFLAGS)
	./test_ana ../img/abigail-keenan-27293.png
	./test_syn ana_q_out_chw.bin ana_IHW_OHW_set.bin

gen_reorder_w_sk_f32c:
	g++ -O3 -w -o test_reorder weight_reorder_sk_f32c.cpp -I . -lm
	./test_reorder	$(LANE_NUM) $(MAX_Tif) $(MAX_Tof)
#f32c end########################################################################################

clean:
	rm -rf *.png
	rm -rf test_*
clean_bin:
	rm -rf *.bin
clean_test:
	rm -f test_*
