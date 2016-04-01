-- lua metatable performance test
-- 2016-04-01
-- xzc

local empty_mt1 = {}
local empty_mt2 = {}
local empty_mt3 = {}
local empty_mt4 = {}
local empty_mt5 = {}
local empty_mt6 = {}
local empty_mt7 = {}
local empty_mt8 = {}
local empty_mt9 = {}

local mt = {}
mt.test = function( a,b ) return a+b end

local mt_tb = {}

setmetatable( empty_mt9,{__index = mt} )
setmetatable( empty_mt8,{__index = empty_mt9} )
setmetatable( empty_mt7,{__index = empty_mt8} )
setmetatable( empty_mt6,{__index = empty_mt7} )
setmetatable( empty_mt5,{__index = empty_mt6} )
setmetatable( empty_mt4,{__index = empty_mt5} )
setmetatable( empty_mt3,{__index = empty_mt4} )
setmetatable( empty_mt2,{__index = empty_mt3} )
setmetatable( empty_mt1,{__index = empty_mt2} )
setmetatable( mt_tb,{__index = empty_mt1} )

local tb = {}
tb.test = function( a,b ) return a+b end



local cnt = 0
for i = 1,100000000 do
    cnt = tb.test( cnt,1 )
end


for i = 1,100000000 do
	cnt = mt_tb.test( cnt,1 )
end

print( "done ====================" )