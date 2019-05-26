-- name.lua
-- xzc
-- 2019-05-26

-- 根据变量取名字
-- lua目前本身没有提供根据变量取名字的函数，即使debug库也没有
-- 但在一起功能，例如定时器回调中，考虑到函数会热更，使用函数名较为合适

local names = {}

local function raw_name( mt,method )
    if not mt then return nil end

    for k,v in pairs( mt ) do
        if v == method then return k end
    end

    -- 当前类找不到，再找基类
    return raw_name( oo.classof(mt),method )
end

-- 这个函数目前只对oo中的对象函数有用，对标C的 __func__ 宏
function __method__( this,method )
    local name = names[method]
    if name then return name end

    assert( this:is_oo(),"only object support" )

    name = raw_name( oo.classof(this),method )

    assert( name,"method name not found" )

    names[method] = name
    return name
end
