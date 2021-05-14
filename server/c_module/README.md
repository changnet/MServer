## C module(.so .dll) can be loaded from here

注意主程序lua库采用static link，如果需要加载lua的so模块，则对应的so模块不能静态链接到lua库，否则报Multiple Lua VMs detected
https://stackoverflow.com/questions/31639483/lua-multiple-vms-detected-while-trying-to-add-extension-for-statically-linke


cjson.so(cjson.dll) just for testing.
```bash
git clone https://github.com/mpx/lua-cjson.git
cd lua-cjson
make
cp cjson.so ../MServer/server/c_module/
```
