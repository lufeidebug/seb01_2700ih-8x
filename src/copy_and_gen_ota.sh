#!/bin/bash

proj_name=seb01_earbuds



#获取当前日期字符串

#dt_str=$(date "+%Y-%m-%d %H:%M:%S")
dt_str=$(date "+%Y%m%d_%H%M")
echo dt_str=$dt_str


#获取固件版本号
fw_ver_file_path=config/${proj_name}/sndp_config.mk
echo cfg_mk_path=${cfg_mk_path}

sw_ver_str=$(grep "SOFTWARE_VERSION" ${fw_ver_file_path})
echo ${sw_ver_str}

sw_ver=${sw_ver_str##*=} 
echo ${sw_ver}

trim_sw=$(echo ${sw_ver} | sed -e 's/^[ ]*//g' | sed -e 's/[ ]*$//g')

#trim_sw=$(echo ${trim_sw:0:-1})
#trim_sw="{trim_sw#"{trim_sw%%[![:space:]]*}"}"   # 去掉前导空格
#trim_sw="{trim_sw%"{trim_sw##*[![:space:]]}"}"  # 去掉尾部空格

echo trim_sw=${trim_sw}



#bin文件源目录
app_bin_src_path=./out/${proj_name}/${proj_name}.bin
#echo app_bin_src=$app_bin_src

#bin文件拷贝到的目录
app_bin_sW=${proj_name}_app_v${trim_sw}.bin
app_bin_ota=${proj_name}_app_for_ota_v${trim_sw}.bin
app_bin_dt=${proj_name}_app_${dt_str}.bin

#echo bin_des=$bin_des


if [ -e $app_bin_src_path ]; then
#    echo "文件存在"
	
	#拷贝bin文件
	echo cp -f ${app_bin_src_path} ./${app_bin_sW}
	cp -f ${app_bin_src_path} ./${app_bin_sW}
	cp -f ${app_bin_src_path} ./${app_bin_dt}
	
	./tools/build_compressed_ota.sh ./${app_bin_sW} ./${app_bin_ota}
	
	rm crc.bin
	rm magic.bin
	
else
    echo "文件不存在"
fi


