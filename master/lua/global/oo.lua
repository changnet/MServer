-- a much simple OO implementation base on loop.simple

oo = {}

local name_class_l = {}       --类列表
local obj_list     = {}       --对象列表

local class_list  = {}        --类列表
local obj_count_l = {}        --对象创建次数列表
local check_count = 0         --check调用次数
local check_flag  = true      --是否记录数据

setmetatable(obj_list, {["__mode"]='k'})


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

--创建c对象
local function cnew(clz)
    local obj = clz:__init() -- 在底层返回一个full userdata

    if check_flag then                  --check
        local name = name_class_l[clz] or "none"
        obj_list[obj] = name
        obj_count_l[name] = (obj_count_l[name] or 0) + 1
    end
    
    return obj
end
--******************************************************************************
-- 声明lua对象
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
    end

    super = super or class_base
    rawset(clz, "__super", super)
    rawset(clz, "__index",clz)    --让自己成为一个metatable
    setmetatable(clz, {__index = super, __call = new})
    return clz
end

-- 声明c对象(super_name多数情况下为nil)
function oo.cclass(super_name, name)
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
    end

    local super = class_list[super_name] or class_base
    rawset(clz, "__super", super)
    rawset(clz, "__index",clz)    --让自己成为一个metatable
    setmetatable(clz, {__index = super, __call = cnew})
    
    -- set loaded and preload so you can require xxx,but not expose class to global
    package.loaded[name]  = clz
    package.preload[name] = clz
    
    return clz
end

-- 获取基类元表
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
function oo.check(log_func)
    collectgarbage("collect")
    
    local c = 0
    local list = {}
    for k,nm in pairs(obj_list) do
        c = c + 1
        list[nm] = (list[nm] or 0) + 1
    end

    check_count = check_count + 1
    local str = string.format("$$$$$$$$$$$$$$$$$$$$$$$$obj size is %d, count is %d\n", c, check_count)
    for nm,cn in pairs(obj_count_l) do
        if list[nm] ~= nil and list[nm] > 1 then
            str = str .. string.format("\t\t%s create %d time,now exist %d!\n", nm, cn, list[nm] or 0)
        end
    end

    if log_func ~= nil then
        log_func(str)
    else
        print(str, string.len(str))
    end
end

return oo
