# MServer

Mini Distributed Game Server(aka Mserver) is a lightweight game engine. It is
designed to be flexible, efficient and easy to use. The engine core is written
in C++ for high performance, and the game logic code is implemented in Lua
scripts to ensure stability, reduce development costs, Increase development
efficiency. The engine provides a lot of components commonly used in game
development, such as navigation, pathfinding, database, etc. Developers can
focus on game logic development instead of re-implementing these components. The
engine is flexible and expandable. It can be deployed as a multi-process to
adapt to MMO-type games, and can also be deployed as a single process for small
games.

## Dependencies
* Linux
* GCC (c++11 or above,check it [here](https://gcc.gnu.org/projects/cxx-status.html))
* MariaDB
* MongoDB
* Lua (5.3 or above)
* protobuf

## Build & Usage

See it at [Wiki](https://github.com/changnet/MServer/wiki/Build)

## Features

 * Hotfix
 * Async log
 * RPC(bson RPC)
 * Lua OOP development
 * Scene(map、navigation、aoi)
 * AC algorithm words filter
 * Crypto(md5、base64、sha1、uuid ...)
 * JSON、XML serialize/deserialize
 * Network(Tcp、Http、websocket), with SSL and ipv6 support
 * Protocol auto serialize/deserialize(protobuf、FlatBuffers)
 * Async and coroutine sync DB operation for MariaDB、MongoDB


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
|              C++ Component                       |
| +--------------+---------------+---------------+ |  thread   +---------+      +----------------+
| |  SSL         |    Socket     |   Protobuf    | <----------->   Log   +------>   Disk Files   |
| +--------------+---------------+---------------+ |           +---------+      +----------------+
| |  Navigation  |   AOI         |  Timer        | |
| +--------------+---------------+---------------+ |  thread   +---------+      +----------------+
| | Crypto       |  JSON/XML     |  RPC          | <-----------> MariaDB +------> MariaDB Server |
| +--------------+---------------+---------------+ |           +---------+      +----------------+
| |              |               |               | |
| +--------------+---------------+---------------+ |  thread   +---------+      +----------------+
| | WordFilter   | FlatBuffers   |               | <-----------> MongoDB +------> MongoDB Server |
| +--------------+---------------+---------------+ |           +---------+      +----------------+
+--------------------------------------------------+
```

## Note

* In latest version, FlatBuffers isn't being tested, may not work properly
* Protobuf library using https://github.com/cloudwu/pbc, some features are NOT the same with Google Protobuf
* using excel to manage config file.https://github.com/changnet/py_exceltools

## Thanks

- http://asciiflow.com/
