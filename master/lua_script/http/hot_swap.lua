-- hot_swap.lua 代理热更新http接口

local HotSwap = {} -- 这个文件不要依赖oo.class，因为热更的时候可能会造成死循环

function HotSwap.exec()
    print( "hot swap exec ====>>>>>>>>>>>>>>>>" )
end

return HotSwap