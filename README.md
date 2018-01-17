# MServer

Mserver,short for Mini Distributed Game Server,is a lightweight game engine. 
This engine contains components like socket、db、log、thread that written in C++ 
to provide high performance.All game logic are designed to run on Lua Script,
improving develop efficiency and stability.Multi Process Architecture and RPC
component make sure it has high scalability.


## Dependencies
* Linux
* G++ >= C++11

## Build(e.g. debian7)

* git clone https://github.com/changnet/MServer.git
* cd MServer
* git submodule update --init --recursive
* cd shell
* ./build_env.sh
* ./make.sh submodule
* ./make.sh

## Usage

Make sure working directory is Mserver/master
```shell
bin/master gateway 1 1
bin/master world 1 1
```
To stop the server,just kill the processes with singal 9.
There are shell scripts(start.sh、stop.sh) at MServer/shell.

There is a client example at:https://github.com/changnet/EgretDemo

## Features

 * Hotswap
 * Multi thread log
 * RPC(bson RPC)
 * Network(Tcp、Http、websocket),all SSL support
 * Lua OOP development
 * Protocol auto serialize/deserialize(Protobuf、FlatBuffers)
 * Async DB operation(MySQL、MongoDB)
 * AC algorithm wordfilter
 * Crypto(md5、base64、sha1、uuid ...)
 * JSON、XML parse/deparse

## Process Architecture

```txt
+--------------------------------------------------+
|                                                  |
|              Lua Game Application                |
|                                                  |
+--------------------------------------------------+
|                                                  |
|              Lua C++ Driver                      |
|                                                  |
+--------------------------------------------------+
| +-------+ +--------------------+ +----------+    |  thread   +---------+      +---------+
| |  SSL  | | Tcp/Http/Websocket | | Protobuf |    <----------->   Log   +------>  Files  |
| +-------+ +--------------------+ +----------+    |           +---------+      +---------+
|                                                  |
| +--------+ +----------+ +-------+                |  thread   +---------+      +---------+
| | Crypto | | JSON/XML | |  RPC  |                <-----------> MySQL   +------>MySQL DB |
| +--------+ +----------+ +-------+                |           +---------+      +---------+
|                                                  |
| +------------+ +-------------+                   |  thread   +---------+      +---------+
| | WordFilter | | FlatBuffers |                   <----------->MongoDB  +------>   DB    |
| +------------+ +-------------+                   |           +---------+      +---------+
+--------------------------------------------------+
```

## ARPG Game Architecture(e.g.)

```txt
              +---------------+
              |               |
              | Web Console   |           +-----------------+
              |               |           |                 |
              +-------^-------+     +----->   Chat Server   +----+
                      |             |     |                 |    |
                      |             |     +-----------------+    |
                      |             |                            |
              +-------v-------+     |     +-----------------+    |
              |               |     |     |                 |    |
Client <------>    GateWay    <----------->   Data Server   +----+
              |               |     |     |                 |    |
              +---------------+     |     +-----------------+    |
                                    |                            |
                                    |     +-----------------+    |
                                    |     | Scene Server 1  |    |
                                    |     +-----------------+    |
                                    +----->       ...       +----+
                                          +-----------------+
                                          | Scene Server N  |
                                          +-----------------+

```

## Valgrind Test

* Valgrind version >= 3.10，valgrind 3.7 on Debian Wheezy not work
* You may want to suppress all memory leak report about OpenSSL with master/valgrind.suppressions

## TODO

* A* search algorithm
* AOI(area of interest)
* Multi Factor sort algorithm(bucket sort、insertion sort)
* DataStruct(LRU、LFU、Priority queue、minimum and maximum heap)

## Note

* In latest version,FlatBuffers isn't being tested,may not work properly
* Protobuf library using https://github.com/cloudwu/pbc, some features are NOT the same with Google Protobuf
* using editor to generate proto file.https://github.com/changnet/SPEditor

## Thanks

- http://asciiflow.com/

