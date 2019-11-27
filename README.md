coordinate
===

Coordinate is a distributed shared memory system for Linux. It is designed to work with Ubuntu 18.04.

To install a Ubuntu 18.04 virtual machine with Vagrant:
```
vagrant box add ubuntu/bionic64
```


Instructions for running Coordinate
```
make
```
To copy the binaries into usr/local
```
sudo make install 
```
To start the Coordinate manager with the user program ./example/test/bin/test
```
coordinate --host <args> ./example/test/bin/test <user program args>
```
To connect a Coordinate peer with the user program ./example/test/bin/test
```
coordinate --host <args> --connect <args> ./example/test/bin/test <user program args>
```
To run a user application in local mode
```
./example/<test program name>/bin/<test program name> 
```
