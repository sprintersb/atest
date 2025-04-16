#!/bin/bash

now=$(date -u +'%F')

dir="avrtest_${now}_mingw32"

if [ ! -f "testavr.h" ]; then
    echo "run this script in avrtest home"
    exit 2
fi

make clean-mingw32
make -j2 all-mingw32

echo "creating ${dir}..."
rm -rf ${dir}
mkdir -p ${dir}/dejagnuboards || exit 1

echo "creating  DATETIME..."
date -u > ${dir}/DATETIME
todos ${dir} DATETIME

for file in *.exe; do
    echo "moving ${file}..."
    mv ${file} ${dir}
done

for file in AUTHORS COPYING README avrtest.h dejagnuboards/exit.c; do
    echo "copying ${file}..."
    cp ${file} ${dir}
    todos ${dir}/${file}
done

zip="${dir}.zip"
echo "creating ${zip}..."
jar cfM ${zip} ${dir}

m5="${zip}.md5"
echo "creating ${m5}..."
md5sum ${zip} > ${m5}

echo "showing ${zip}..."
echo
jar tf ${zip}

echo "uploading ${dir}..."
upload-avrtest.sh ${dir}
