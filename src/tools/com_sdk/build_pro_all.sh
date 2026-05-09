#!/bin/bash
set -e

chmod 777 tools/*
chmod 777 tools/com_sdk/*
chmod 777 tools/com_sdk/cust/*

rm -rf out/*
./tools/com_sdk/build_cust_cmd.sh best1306p
./tools/com_sdk/build_cust_cmd.sh best1502p
./tools/com_sdk/build_cust_cmd.sh best1503
./tools/com_sdk/build_cust_cmd.sh best1306
./tools/com_sdk/build_cust_cmd.sh best1503p
./tools/com_sdk/cust/build_pro_1306_0001.sh
./tools/com_sdk/cust/build_pro_1306p_0001.sh
./tools/com_sdk/cust/build_pro_1306p_0002.sh
./tools/com_sdk/cust/build_pro_1306p_0005.sh best1306p
./tools/com_sdk/cust/build_pro_1306p_0006.sh best1306p
./tools/com_sdk/cust/build_pro_1306p_0007.sh best1306p
./tools/com_sdk/cust/build_pro_1306p_0008.sh
./tools/com_sdk/cust/build_pro_1306p_0009.sh
./tools/com_sdk/cust/build_pro_1306p_0010.sh
./tools/com_sdk/cust/build_pro_1502p_0001.sh
./tools/com_sdk/cust/build_pro_1502p_0002.sh
./tools/com_sdk/cust/build_pro_1502p_0003.sh
./tools/com_sdk/cust/build_pro_1502p_0004.sh
./tools/com_sdk/cust/build_pro_1502p_0005.sh
./tools/com_sdk/cust/build_pro_1502p_0006.sh
./tools/com_sdk/cust/build_pro_1502p_0007.sh best1502p
./tools/com_sdk/cust/build_pro_1502p_0008.sh
./tools/com_sdk/cust/build_pro_1502p_0009.sh
./tools/com_sdk/cust/build_pro_1502p_0010.sh best1502p
./tools/com_sdk/cust/build_pro_1502p_0011.sh best1502p
./tools/com_sdk/cust/build_pro_1502p_0012.sh
./tools/com_sdk/cust/build_pro_1502p_0013.sh
./tools/com_sdk/cust/build_pro_1503_0001.sh
./tools/com_sdk/cust/build_pro_1503_0002.sh best1503
./tools/com_sdk/cust/build_pro_1503_0003.sh
./tools/com_sdk/cust/build_pro_1503_0005.sh best1503
./tools/com_sdk/cust/build_pro_1503_0006.sh best1503
# ./tools/com_sdk/build_cust_cmd.sh best1307p
# ./tools/besui_tool/build.sh TWSPRO OTA
# ./tools/besui_tool/build.sh NOTWS OTA