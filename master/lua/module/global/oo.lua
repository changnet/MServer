-- a much simple OO implementation base on loop.simple

module("oo", package.seeall)

--************内存检测***********
local name_class_l = {}       --类列表
local obj_list     = {}       --对象列表

setmetatable(obj_list, {["__mode"]='k'})

local obj_count_l = {}     --对象创建次数列表
local check_count = 0
local check_flag  = false

function open_check_flag()
	check_flag = true
end

function close_check_flag()
	check_flag = false
end

function check(log_func)
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


--***************oo******************
local class_list = {}  --类列表
local class_base = {}  --基类

local function table_copy(src, dst, copy)
	dst = dst or {}
	copy = copy or {}
	assert(type(src) == "table" and type(dst) == "table" and type(copy) == "table")
	copy[src]=dst
	local k, v
	for k, v in pairs(src) do
		if type(v) == "table" then
			dst[k] = copy[v] or table_copy(v, {}, copy)
		else
			dst[k] = v
		end
	end
	return dst
end
table.copy = table_copy


-- default construnctor
function class_base:__init(ob)
	if ob ~= self and type(ob) == "table" then
		table_copy(ob, self)
	end
end

-- isa operator
function class_base:isa(clz)
	local c = classof(self)
	while c and c ~= class_base do
		if c == clz then return true end
		c = superclassof(c)
	end
	return false
end

local function new(clz, ...)
	local obj = {}
	local mt = rawget(clz, "_mt")
	if not mt then
		mt = {
			__index		= clz,
			__tostring	= clz.__tostring,
			__add		= clz.__add,
			__sub		= clz.__sub,
			__mul		= clz.__mul,
			__div		= clz.__div,
			__mod		= clz.__mod,
			__pow		= clz.__pow,
			__unm		= clz.__unm,
			__concat	= clz.__concat,
			__len		= clz.__len,
			__eq		= clz.__eq,
			__lt		= clz.__lt,
			__le		= clz.__le,
			__call		= clz.__call,
			__gc		= clz.__gc,
		}
		rawset(clz, "_mt", mt)
	end
	setmetatable(obj, mt)
	obj:__init(...)

	if check_flag then                  --check
		local name = name_class_l[clz] or "none"
		obj_list[obj] = name
		obj_count_l[name] = (obj_count_l[name] or 0) + 1
	end

	return obj
end

function class(super, name)
	local clz = {}
	if type(name) == "string" then
		local class_name = name..'__'
		if class_list[class_name] ~= nil then
			clz = class_list[class_name]
			for k,v in pairs(clz) do
				clz[k] = nil
			end
		else
			class_list[class_name] = clz
		end

		if check_flag then                     --check
			name_class_l[clz] = class_name
		end
	end

	super = super or class_base
	rawset(clz, "__super", super)
	setmetatable(clz, {__index = super, __call = new})
	return clz
end

function superclassof(clz)
	return rawget(clz, "__super")
end

function classof(ob)
	return getmetatable(ob).__index
end

function instanceof(ob, clz)
	return ((ob.isa and ob:isa(clz)) == true)
end

local cpp_class = class(nil, "cpp_class")
function cpp_class:New(...)
	return self(...)
end

function cpp_class:Destroy(...)
	if type(self._cobj_) == "userdata" then
		self._cobj_:__gc()
	end
end

function cclass()
	return class(cpp_class)
end

