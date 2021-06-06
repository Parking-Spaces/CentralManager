### Note:
It´s required to have the proto file in: ../CentralProto/parkingspaces.proto (Just clone the proto repo in the parent folder to this project)


## Install curlpp with:
sudo apt-get install pkg-config libcurlpp-dev libcurl4-openssl-dev sqlite3 libsqlite3-dev

## Then create the build directory, make files and compile using:

```bash
$ mkdir build && cd build 
```
```bash
$ cmake ..
```
```bash
$ make
```

### Then you can run the server which should be in the binary named Raspberry
