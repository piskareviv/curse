# !!! do not run this on your personal computer !!!

sudo apt-get update
sudo apt-get install -y cmake build-essential
sudo apt-get install -y clang-tidy clang-format
        
wget -qO /tmp/llvm.sh https://apt.llvm.org/llvm.sh
chmod +x /tmp/llvm.sh
sudo /tmp/llvm.sh 20 all

clang-20 --version

# echo 'CC="clang-20"'    | sudo tee -a /etc/environment
# echo 'CXX="clang++-20"' | sudo tee -a /etc/environment
