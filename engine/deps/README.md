此目录包含依赖的第三方库，部分是submodule，部分直接放源码。源码包小，并且有定期发布版本的直接放源码，比submodule方便。

 
各个源码下载地址
---------------
* [llhttp v6.0.5](https://github.com/nodejs/llhttp/tags)
* [kcp 20260516 - v2.1.1](https://github.com/skywind3000/kcp)
* [lua v5.5.1](https://www.lua.org/ftp/)，放在`lua`目录，由cmake编译，不使用vcpkg或者系统的lua
* [lua-cjson 2.1.0.17](https://github.com/openresty/lua-cjson)，放在`lua-cjson`目录，由cmake编译成c模块
