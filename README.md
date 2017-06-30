# Open SoC Debug Software

This is the reference implementation of the Open SoC Debug host software.

## Build Dependencies
```sh
sudo apt-get install libzmq3-dev libczmq3-dev
```

## Build
```sh
./autogen.sh
mkdir build
cd build
../configure
make 
make install
```
