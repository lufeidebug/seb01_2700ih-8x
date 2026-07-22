#!/bin/bash
export PATH=/usr/bin/gcc-arm-none-eabi-10.3-2021.07/bin:$PATH

rm ./out/seb01_earbuds/seb01_earbuds.bin

#./tools/com_sdk/cust/build_pro_1503_0003_seb01.sh $1
./tools/com_sdk/cust/build_pro_1503_0001_seb01.sh $1


./copy_and_gen_ota.sh