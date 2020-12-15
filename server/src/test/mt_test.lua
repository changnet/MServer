-- lua metatable performance test
-- 2016-04-01
-- xzc

--[[
测试结果表明，使用元表速度更慢。因此这套框架的继承一般不要超过3层。
local ts = 10000000
call function native	1091772	microsecond
call function as table value	1287172	microsecond
call function with 3 level metatable	2014431	microsecond
call function with 10 level metatable	3707181	microsecond
]]

local test = function( a,b ) return a+b end

local empty_mt1 = {}
local empty_mt2 = {}
local empty_mt3 = {}
local empty_mt4 = {}
local empty_mt5 = {}
local empty_mt6 = {}
local empty_mt7 = {}
local empty_mt8 = {}

local mt = {}
mt.test = test

local mt_tb = {}

setmetatable( empty_mt8,{__index = mt} )
setmetatable( empty_mt7,{__index = empty_mt8} )
setmetatable( empty_mt6,{__index = empty_mt7} )
setmetatable( empty_mt5,{__index = empty_mt6} )
setmetatable( empty_mt4,{__index = empty_mt5} )
setmetatable( empty_mt3,{__index = empty_mt4} )
setmetatable( empty_mt2,{__index = empty_mt3} )
setmetatable( empty_mt1,{__index = empty_mt2} )
setmetatable( mt_tb,{__index = empty_mt1} )

local tb = {}
tb.test = test

local ts = 10000000

f_tm_start()
local cnt = 0
for _ = 1,ts do
    cnt = test( cnt,1 )
end
f_tm_stop( "call function native" )

f_tm_start()
cnt = 0
for _ = 1,ts do
    cnt = tb.test( cnt,1 )
end
f_tm_stop( "call function as table value" )

f_tm_start()
for _ = 1,ts do
    cnt = empty_mt6.test( cnt,1 )
end
f_tm_stop( "call function with 3 level metatable" )

f_tm_start()
for _ = 1,ts do
    cnt = mt_tb.test( cnt,1 )
end
f_tm_stop( "call function with 10 level metatable" )
