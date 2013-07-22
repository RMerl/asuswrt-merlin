#!/bin/sh

TEST_DIR=$TEST_FILE_SYSTEM_MOUNT_DIR
if test -z "$TEST_DIR";
then
	TEST_DIR="/mnt/test_file_system"
fi

rm -rf ${TEST_DIR}/*

./simple/test_1 || exit 1

rm -rf ${TEST_DIR}/*

./simple/test_2 || exit 1

rm -rf ${TEST_DIR}/*

./integrity/integck || exit 1

rm -rf ${TEST_DIR}/*

./stress/atoms/rndrm00 -z0 || exit 1

rm -rf ${TEST_DIR}/*

./stress/atoms/rmdir00 -z0 || exit 1

rm -rf ${TEST_DIR}/*

./stress/atoms/stress_1 -z10000000 -e || exit 1

rm -rf ${TEST_DIR}/*

./stress/atoms/stress_2 -z10000000 || exit 1

rm -rf ${TEST_DIR}/*

./stress/atoms/stress_3 -z1000000000 -e || exit 1

rm -rf ${TEST_DIR}/*

cd stress || exit 1

./stress00.sh 360 || exit 1

./stress01.sh 360 || exit 1

cd .. || exit 1
