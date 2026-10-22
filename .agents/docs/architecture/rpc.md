# rpc调用
1. rpc的路由规则统一由server\src\router\router.lua控制，rpc调用必须从路由中获取一个地址
2. 一些常用的地址在server\src\engine\preloader.lua中有定义，比如DATA_ADDR，可直接使用
3. rpc的逻辑在server\src\rpc\rpc.lua中，一般用法为
```lua
Call.DataCache.get(DATA_ADDR, "player", PLAYER_KEYS)
Send.ClusterProxy.on_request(from_addr, LOCAL_ADDR, to_addr)
```
