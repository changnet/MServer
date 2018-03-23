-- a much simple OO implementation base on loop.simple

oo = {}

local name_class_l = {}       -- 类列表，类为k，name为v
local class_list   = {}       -- 类列表，name为k，类为v
local obj_list     = {}       -- 已创建对象列表，对象为k，name为value
local singleton    = {}       -- 单例，类为k，v为实例

local obj_count_l = {}        -- 对象创建次数列表
local check_count = 0         -- check调用次数
local check_flag  = true      -- 是否记录数据

setmetatable(obj_list, {["__mode"]='k'})
setmetatable(singleton, {["__mode"]='v'})

--******************************************************************************
local class_base = {}  --默认基类
-- default construnctor
function class_base:__init()
end

-- isa operator(是否从某个类继承而来或者就是该类)
function class_base:isa(clz)
    local c = classof(self)
    while c and c ~= class_base do
        if c == clz then return true end
        c = superclassof(c)
    end
    return false
end
--******************************************************************************
-- 创建lua对象
local function new(clz, ...)
    local obj = {}

    setmetatable(obj, clz)
    obj:__init(...)

    if check_flag then                  --check
        local name = name_class_l[clz] or "none"
        obj_list[obj] = name
        obj_count_l[name] = (obj_count_l[name] or 0) + 1
    end

    return obj
end

-- 创建单例
local function new_singleton( clz,... )
    if not singleton[clz] then
        singleton[clz] = new( clz,... )
    end

    return singleton[clz]
end

--******************************************************************************
-- 声明lua类
function oo.class(super, name)
    local clz = {}
    if type(name) == "string" then
        if class_list[name] ~= nil then
            clz = class_list[name]
            for k,v in pairs(clz) do  --这是热更的关键
                clz[k] = nil
            end
        else
            class_list[name] = clz
        end

        if check_flag then                     --check
            name_class_l[clz] = name
        end
    else
        error( "oo class no name specify" )
        return
    end

    super = super or class_base
    rawset(clz, "__super", super)
    -- 设置metatable的__index,创建实例(调用__call)时让自己成为一个metatable
    rawset(clz, "__index",clz)
    -- 设置自己的metatable为父类，这样才能调用父类函数
    setmetatable(clz, {__index = super, __call = new})
    return clz
end

-- 声明lua单例类
function oo.singleton(super, name)
    local clz = {}
    if type(name) == "string" then
        if class_list[name] ~= nil then
            clz = class_list[name]
            for k,v in pairs(clz) do  --这是热更的关键
                clz[k] = nil
            end
        else
            class_list[name] = clz
        end

        if check_flag then                     --check
            name_class_l[clz] = name
        end
    else
        error( "oo singleton no name specify" )
        return
    end

    super = super or class_base
    rawset(clz, "__super", super)
    rawset(clz, "__index",clz)       --让自己成为一个metatable
    rawset(clz, "__singleton",true)
    setmetatable(clz, {__index = super, __call = new_singleton})
    return clz
end

--******************************************************************************

-- 获取基类元表(clz是元表而不是对象)
function oo.superclassof(clz)
    return rawget(clz, "__super")
end

-- 获取当前类元表
function oo.classof(ob)
    return getmetatable(ob).__index
end

-- 获取某个类的元表
function oo.metatableof(name)
    return class_list[name]
end

-- 内存检查
function oo.check( log_func )
    collectgarbage("collect")

    local obj_size = 0
    local list = {}
    for obj,name in pairs(obj_list) do
        obj_size = obj_size + 1
        list[name] = (list[name] or 0) + 1
    end

    check_count = check_count + 1
    local str = string.format("obj size is %d, check times is %d\n", obj_size, check_count)
    for name,ts in pairs(obj_count_l) do
        if list[name] and list[name] > 0 then
            str = str .. string.format("\t%s create %d time,now exist %d!\n",name,ts,list[name] or 0)
        end
    end

    str = str .. "check done ... \n"
    if log_func then
        log_func( str )
    else
        print( str )
    end
end

-- 热更
function oo.hot_fix( log_func )
    log_func = log_func or print

    for k,_ in pairs( class_list ) do
        require( k )
        log_func( "hot fix " .. k )
    end

    for k,_ in pairs( const_define ) do
        require( k )
        log_func( "hot fix " .. k )
    end
end

return oo
