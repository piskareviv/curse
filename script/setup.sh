#!/usr/bin/bash


# !!! do not run this on your personal computer !!!

apt update
apt install -y cmake build-essential
apt install -y clang-tidy clang-format
apt install -y libabsl-dev libgtest-dev libbenchmark-dev
apt install -y libre2-dev
apt install -y pkg-config


wget -qO /tmp/llvm.sh https://apt.llvm.org/llvm.sh
chmod +x /tmp/llvm.sh
/tmp/llvm.sh 20 all

clang-20 --version

# echo 'CC="clang-20"'    | sudo tee -a /etc/environment
# echo 'CXX="clang++-20"' | sudo tee -a /etc/environment
