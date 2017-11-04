# Open SoC Debug Software

[![Run Status](https://api.shippable.com/projects/59fddeb2102c2107006ff9b9/badge?branch=master)](https://app.shippable.com/github/imphil/osd-software) 

This is the reference implementation of the Open SoC Debug host software.

## Build Dependencies
```sh
./install-build-deps.sh
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
