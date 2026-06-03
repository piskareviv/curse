#!/usr/bin/bash


FILES=$(find src/ -name "*.cpp" -o -name "*.hpp")

clang-format --dry-run --Werror $FILES || exit 1
clang-tidy -p ./build $FILES --quiet --header-filter="src/.*" || exit 1
echo "CODESTYLE CHECK PASSED"
