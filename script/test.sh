#!/usr/bin/bash


build_folder=$1

for test in `ls $build_folder/bin/*_test`; do
    echo RUNNING $test
    if ! ./$test; then
        echo TEST $test FAILED
        exit 1
    fi
done

