#!/usr/bin/bash

(mkdir -p build_asan && cd build_asan && cmake -DCMAKE_BUILD_TYPE=ASAN    .. && make -j7) && bash script/test.sh build_asan &&
(mkdir -p build      && cd build      && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j7) && bash script/test.sh build      
