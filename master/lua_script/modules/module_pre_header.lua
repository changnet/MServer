-- module_pre_header.lua
-- 2018-01-26
-- xzc

-- 引入高优先级的文件

require "global.oo" -- 这个文件优先级最高，必须在require之前,绝对不要热更到
require "global.require"

-- 引用一起基础文件，但可以热更的。其他逻辑初始化时可能会用到这些库
require "global.global"
require "global.table"
require "global.string"
require "modules.system.define"

g_unique_id = require "modules.system.unique_id"
