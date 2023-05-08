# usbcommuni
USB通讯模块 (Linux to Android / IOS)

# compile
## linux pc
- cmake .. -DCMAKE_PREFIX_PATH=/home/kaisen/wk/opt/host-local -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

## raspberry 4B
- cmake .. -DCMAKE_PREFIX_PATH=/home/kaisen/wk/opt/host-debian -DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-g++
