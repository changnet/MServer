# MServer (Mini Distributed Game Server)

**MServer** 是一个轻量级、灵活可扩展、高效且易用的游戏服务器引擎。

- **高性能**：底层基于 C++ 实现，保证运行性能。提供完善的构建系统和`Lua Binding`，方便开发有性能要求的组件。
- **稳定性**：游戏业务逻辑采用 Lua 脚本进行开发，降低崩溃宕机的概率，保持极高的开发效率高。
- **扩展性**：可以在多线程、多进程、集群模式中切换，根据需求扩展线程节点数量。

## 核心功能
- Lua热更新
- Lua面向对象编程（可选）
- 网络：Tcp、Http、Websocket，支持SSL
- RPC（支持协程、异步回调）
- Protobuf
- 数据库：MySQL/MariaDB、MongoDB
- JSON、XML、AOI、AStar、关键字过滤等游戏开发常用组件

## 进程架构

MServer 在设计上采用了**多进程多线程**的架构，由业务需求灵活配置进程与线程的数量，单个进程主要包含以下几个核心部分：
1. **一个网络线程 (Network Thread)**：负责底层网络数据的收发与解析。
2. **一个日志线程 (Log Thread)**：负责异步处理日志。
3. **一个主线程 (Main Thread)**：作为管理者，负责管理以及协调其它工作线程。
4. **N 个工作线程 (Worker Threads)**：根据业务可开启0个或者N个线程，每个 Worker 线程都拥有一个**独立隔离的 Lua 虚拟机**执行业务逻辑。

```mermaid
graph LR
    subgraph ThreadModel ["MServer 全局多线程模型"]
        direction TB
        Net["🌐 Net 网络线程"]
        Log["📝 Log 日志线程"]
        Main["👑 Main 线程 (管理/调度)"]

        subgraph WorkerPool ["⚙️ Worker 工作线程 (并行处理节点)"]
            direction TB
            Gateway["Gateway (网关节点)"]
            Database["Database (数据节点)"]
            Player1["Player 1 (玩家业务1)"]
            Player2["Player 2 (玩家业务2)"]
            OtherWorkers["...(其他业务线程)"]
        end
        Net ~~~ Log ~~~ Main ~~~ WorkerPool
    end

    subgraph ThreadArch ["单个线程独立内部架构 (一个线程一个Lua VM)"]
        direction BT

        subgraph CppLayer ["🛠 C++ 底层核心框架"]
            CoreComps["基础组件<br/>(Timer, Protobuf, AOI, Socket, Crypto, JSON...)"]
        end

        subgraph LuaLayer ["📜 Lua 业务逻辑层"]
            LuaComps["游戏常用模块、组件<br/>(time，co_pool，login，bag...)"]
            BizLogic["Custom Business Code<br/>(具体业务逻辑)"]
        end

        CppLayer -- "Lua Cpp Binding(lpp)" --> LuaLayer
    end

    %% 关联指示，表示这些线程都遵循这套内部隔离架构
    Main == "主线程" ==> ThreadArch
    Gateway == "worker线程" ==> ThreadArch
    Database == "worker线程" ==> ThreadArch
    Player1 == "worker线程" ==> ThreadArch
    Player2 == "worker线程" ==> ThreadArch
    OtherWorkers == "worker线程" ==> ThreadArch

    %% 样式表
    classDef core fill:#ffe0b2,stroke:#ff9800,stroke-width:2px,color:#000
    classDef worker fill:#e1f5fe,stroke:#039be5,stroke-width:2px,color:#000
    classDef cpp fill:#f1f8e9,stroke:#7cb342,stroke-width:2px,color:#000
    classDef lua fill:#fff9c4,stroke:#fbc02d,stroke-width:2px,color:#000

    class Net,Log,Main core
    class Gateway,Database,Player1,Player2,OtherWorkers worker
    class CppLayer,CoreComps cpp
    class LuaLayer,BizLogic lua
```

## 典型单服部署架构

```mermaid
graph LR
    subgraph Clients ["📱 客户端集群"]
        direction TB
        C1["客户端 1"]
        C2["客户端 2"]
        C3["客户端 3"]
        C4["..."]

        C1 ~~~ C2
        C2 ~~~ C3
        C3 ~~~ C4
    end

    subgraph Servers ["📱 服务器"]
        direction LR

        subgraph GatewayProcess ["🛡️ Gateway 进程 (网关服)"]
            direction TB
            GW_Net["底层网络收发"]
            GW_Login["Login 登录与认证"]

            GW_Net ~~~ GW_Login
        end

        subgraph Backend [" "]
            direction TB

            subgraph GameProcess ["⚔️ Game 进程 (逻辑服)"]
                direction TB
                GameLogic["全局游戏业务线程"]

                subgraph Players ["Player Workers (多节点并发处理玩家业务)"]
                    direction TB
                    P1["Player 1"]
                    P2["Player 2"]
                    P3["Player 3..."]

                    P1 ~~~ P2
                    P2 ~~~ P3
                end

                subgraph Scenes ["Scene Workers (节点并发处理场景)"]
                    direction TB
                    S1["Scene 1"]
                    S2["Scene 2"]
                    S3["Scene 3..."]

                    S1 ~~~ S2
                    S2 ~~~ S3
                end

                OtherThreads["其他业务线程..."]

                GameLogic ~~~ Players
                Players ~~~ Scenes
                Scenes ~~~ OtherThreads
            end

            subgraph DataProcess ["💾 Data 进程 (数据服)"]
                direction TB
                D_Cache["数据缓存层 (Cache)"]
                D_DB["数据库读写调度 (DB I/O)"]

                D_Cache ~~~ D_DB
            end
        end
    end

    Clients <-->|"外网直连"| GatewayProcess
    GatewayProcess == "RPC" <==> GameProcess
    GatewayProcess == "RPC" <==> DataProcess
    GameProcess == "RPC" <==> DataProcess

    %% 样式表
    classDef client fill:#f1f8e9,stroke:#7cb342,stroke-width:2px,color:#000
    classDef gateway fill:#e1f5fe,stroke:#039be5,stroke-width:2px,color:#000
    classDef game fill:#fff8e1,stroke:#ffa000,stroke-width:2px,color:#000
    classDef dataserver fill:#fce4ec,stroke:#d81b60,stroke-width:2px,color:#000
    classDef backend fill:none,stroke:none

    class C1,C2,C3,C4 client
    class GatewayProcess gateway
    class GameProcess game
    class DataProcess dataserver
    class Backend backend
```

上面是一个典型的单服游戏服务器架构。**网关**作为一个独立进程负责网络数据以及认证，即使被攻击崩溃也不会影响业务逻辑。**数据缓存及数据库读写**也在一个独立进程，即使业务逻辑进程崩溃，也能保证数据安全。而**游戏逻辑**进程中根据业务需求开启多个线程，保证并发又避免多进程部署复杂的问题。





# 其他
* 更多功能介绍[Wiki](https://github.com/changnet/MServer/wiki)
* 配置导表工具：https://github.com/changnet/exceltools
