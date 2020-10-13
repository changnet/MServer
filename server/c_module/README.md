## C module(.so) can be loaded from here

注意主程序lua库采用static link，如果需要加载lua的so模块，则对应的so模块不能静态链
接到lua库，否则报Multiple Lua VMs detected
https://stackoverflow.com/questions/31639483/lua-multiple-vms-detected-while-trying-to-add-extension-for-statically-linke


cjson.so from ``luarocks install lua-cjson 2.1.0-1`` just for testing.
