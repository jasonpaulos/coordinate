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

To install the shared libraries into /usr/local
```
sudo make install 
```

To start the Coordinate manager with the user program ./example/dotproduct/bin/dotproduct
```
coordinate --host <args> --cores <number of machines> ./example/dotproduct/bin/dotproduct <user program args>
```

To connect a Coordinate peer with the user program ./example/dotproduct/bin/dotproduct
```
coordinate --host <args> --connect <args> ./example/dotproduct/bin/dotproduct
```

To run a user application in local mode
```
./example/dotproduct/bin/dotproduct <user program args>
```
