#!/usr/bin/bash


# !!! do not run this on your personal computer !!!

apt-get update
apt-get install -y cmake build-essential
apt-get install -y clang-tidy clang-format
apt install libabsl-dev libgtest-dev libbenchmark-dev
apt install libre2-dev
apt install pkg-config


wget -qO /tmp/llvm.sh https://apt.llvm.org/llvm.sh
chmod +x /tmp/llvm.sh
/tmp/llvm.sh 20 all

clang-20 --version

# echo 'CC="clang-20"'    | sudo tee -a /etc/environment
# echo 'CXX="clang++-20"' | sudo tee -a /etc/environment
