-- preload.lua
-- 预加载lua脚本,preload优先级很高，仅在lua堆栈初始化之后，在C底层初始化之前。
-- 非必要文件或者需要调用C底层(如DB、NET...)的lua文件不允许在此加载。
-- xzc 2015-09-09

require "lua.global.oo"
require "lua.global.global"
