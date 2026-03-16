#!/bin/bash
set -e

chmod 777 tools/*
chmod 777 tools/com_sdk/*
chmod 777 tools/com_sdk/cust/*

rm -rf out/*
./tools/com_sdk/build_cust_cmd.sh best1306
./tools/com_sdk/build_cust_cmd.sh best1306p
./tools/com_sdk/build_cust_cmd.sh best1502p
./tools/com_sdk/build_cust_cmd.sh best1503
./tools/com_sdk/build_cust_cmd.sh best1503p
./tools/com_sdk/cust/build_pro_1306p_0001.sh
./tools/com_sdk/cust/build_pro_1306p_0002.sh
./tools/com_sdk/cust/build_pro_1306_0001.sh
./tools/com_sdk/cust/build_pro_1502p_0001.sh
# ./tools/com_sdk/build_cust_cmd.sh best1307p
# ./tools/besui_tool/build.sh TWSPRO OTA
# ./tools/besui_tool/build.sh NOTWS OTA