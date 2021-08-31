-- name.lua
-- xzc
-- 2019-05-26
-- 根据变量取名字
-- lua目前本身没有提供根据变量取名字的函数，即使debug库也没有
-- 但在一些功能，例如定时器回调中，考虑到函数会热更，使用函数名较为合适
-- 这里提供一个机制取对象(oo.lua)中的成员函数，做了缓存之后，也不会有效率问题
-- 如果是local函数，那没有通用的方法可以取，只能通过手动注册。
-- 这个仅限于定时器、rpc等特殊调用，一般情况下，还是用对象来做吧

local method_names = {}
local func_names = {}
local names_func = {}

local obj_names = {}
local names_obj = {}

-- 从对象元表里查找成员函数的名字
-- @param mt 对象的元表
-- @param method 需要查找的函数名字
-- @return  函数的名字
local function find_method_name(mt, method)
    if not mt then return nil end

    for k, v in pairs(mt) do if v == method then return k end end

    -- 当前类找不到，再找基类
    return find_method_name(oo.classof(mt), method)
end

-- 取对象中的函数名，目前只对oo中的对象函数有用，类似C的 __func__ 宏
-- @param this 通过oo创建的对象
-- @param method 需要查找的函数名字
-- @return  函数的名字
function method_name(this, method)
    local name = method_names[method]
    if name then
        if -1 == name then return nil end -- 已查找过但找不到名字的，标为-1
        return name
    end

    -- 当前仅仅支持oo里的对象
    if "table" ~= type(this) or not this.is_oo then
        method_names[method] = -1
        return nil
    end

    name = find_method_name(oo.classof(this), method)

    method_names[method] = name or -1
    return name
end

-- 成员函数调用及其参数转换为一个函数调用，类似C++中的std::bind
-- @param this 对象
-- @param method 成员函数名
-- @param p0 ... 其他回调参数，不需要可以不选择
function method_thunk(this, method, p0, p1, p2, p3, p4, p5)
    -- p0, p1 ... 可变参数，因为不能用...，
    -- cannot use '...' outside a vararg function near '...'
    -- 但也没必要用table.pack

    -- 需要通过函数名去调用，因为函数可能被热更
    local name = assert(method_name(this, method), "no method found")

    -- method用不着了，这里没必要继承引用旧函数，不然热更的时候不会gc
    -- ps:高版本的lua(5.3)会自动判断这些upvalue是否被引用。而低版本则不会
    -- 传入不同的参数到闭包，如果这个参数没用到，lua 5.3返回的函数指针是一样的，5.1的不一样
    method = nil
    return function()
        return this[name](this, p0, p1, p2, p3, p4, p5)
    end
end

-- 把一个函数调用及其参数转换为一个函数保存起来
function func_thunk(func, p0, p1, p2, p3, p4, p5)
    local name = func_names[func]
    if not name then
        return function()
            return func(p0, p1, p2, p3, p4, p5)
        end
    end

    -- 能查找到名字，说明这个函数需要支持热更
    return function()
        local cb = assert(names_func[name], name)
        return cb(p0, p1, p2, p3, p4, p5)
    end
end

-- 注册一个函数名
function reg_func(name, func)
    -- 唯函数名和函数指针是一一对应的
    assert(not func_names[func], name)
    assert(not names_func[name], name)

    func_names[func] = name
    names_func[name] = func
end

-- 注册一个对象
function reg_obj(name, obj)
    assert(not obj_names[obj], name)
    assert(not names_obj[name], name)

    obj_names[obj] = name
    names_obj[name] = obj
end

-- 取函数名，仅对手动注册过的函数有效，类似C的 __func__ 宏
-- 要么是全局函数、二级函数或者手动注册的函数才取得到名字
function func_to_name(func)
    return func_names[func]
end

-- 根据函数名取函数指针
function name_to_func(name)
    return names_func[name]
end

-- 根据对象指针取对象名字
function obj_to_name(obj)
    return obj_names[obj]
end

-- 根据对象指针取对象名字
function name_to_obj(name)
    return names_obj[name]
end

-- 通过遍历全局表，生成函数及其对应的名字
function make_name()
    -- 这个函数尽量在所有模块加载完成之后，配置、数据对象创建之前调用，避免搜索过多无效的数据
    -- 这里仅处理全局函数和二级函数，其他的太多处理不过来
    local tm = os.clock()

    -- 做名字、指针映射是为了方便rpc、定时器等回调，因此需要名字和指针一一对应
    -- 但有不少模块其实是有同名、同函数的情况（如math.atan和math.atan2是同一个函数）
    -- 因此这里排除一些有冲突的模块，反正在rpc、定时器里它们也用不到
    local exclude = {
        _G = true, -- 有一个_G._G需要排除，https://www.lua.org/pil/14.html
        os = true,
        io = true,
        utf8 = true,
        math = true, -- atan和atan2是同一个函数
        table = true,
        debug = true,
        __test = true,
        string = true,
        coroutine = true,
        json = true,
        lua_parson = true, -- 重命名为json了
        lua_rapidxml = true,
    }

    local count = 0
    local reg_o = reg_obj
    local reg_f = reg_func
    for name, value in pairs(_G) do
        local t = type(value)
        if "function" == t then
            reg_f(name, value)
            count = count + 1
        elseif "table" == t and not exclude[name] then
            reg_o(name, value)
            for sub_name, sub_value in pairs(value) do
                if "function" == type(sub_value) then
                    -- 不同模块可能会有同名函数，重名时尝试拼模块名，效率略低
                    if names_func[sub_name] then
                        reg_f(name .. "." .. sub_name, sub_value)
                    else
                        reg_f(sub_name, sub_value)
                    end
                    count = count + 1
                end
            end
        end
    end
    printf("make name done, %d functions in %.2f sec",
        count, os.clock() - tm)
end
