#!/usr/bin/bash

(mkdir -p build_asan && cd build_asan && cmake -DCMAKE_BUILD_TYPE=ASAN     .. && make -j7) && bash script/test.sh build_asan &&
(mkdir -p build_tsan && cd build_tsan && cmake -DCMAKE_BUILD_TYPE=TSAN     .. && make -j7) && bash script/test.sh build_tsan &&
# (mkdir -p build_msan && cd build_msan && cmake -DCMAKE_BUILD_TYPE=MSAN     .. && make -j7) && bash script/test.sh build_msan &&
(mkdir -p build      && cd build      && cmake -DCMAKE_BUILD_TYPE=Release  .. && make -j7) && bash script/test.sh build      &&
(mkdir -p build_cov  && cd build_cov  && cmake -DCMAKE_BUILD_TYPE=COVERAGE .. && make -j7) && bash script/test.sh build_cov  &&
echo ALL TESTS PASSED
