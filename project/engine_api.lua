-- 导出引擎中的接口，用于代码提示和luacheck

-- 在project目录执行 lua engine_api.lua


-- 入口文件，所有导出的符号都是从这里开始查找的
local entrance = "../engine/src/lpp/llib.cpp"

-- C++源码目录，要导出的符号肯定在这个目录的源码中
local dir = "../engine/src/"

-- 导出目录
local export_dir = "../server/engine/"

local function e_traceback()
    return debug.traceback()
end

local function to_readable( val )
    if type(val) == "string" then
        return "\"" .. val .. "\""
    end
    return val
end

-- 提取名字空间的辅助函数
local function split_namespace(cpp_symbol)
    local ns, name = string.match(cpp_symbol, "^(.*)::(.*)$")
    if not ns then return "", cpp_symbol end
    return ns, name
end

-- 读取整个文件
local function read_file(path)
    local f = io.open(path, "r")
    if not f then return nil end
    local c = f:read("*a")
    f:close()
    return c
end

-- 获取C++类型的Lua映射
local function map_type(ctype)
    ctype = string.gsub(ctype, "const%s+", "")
    ctype = string.gsub(ctype, "%s*&%s*", "")
    ctype = string.gsub(ctype, "%s*%*%s*", "")
    ctype = string.match(ctype, "^%s*(.-)%s*$") or ctype
    
    if ctype == "int" or ctype == "int32_t" or ctype == "uint32_t" or ctype == "int64_t" or ctype == "uint64_t" 
       or ctype == "float" or ctype == "double" or ctype == "size_t" or ctype == "short" or ctype == "long" then
        return "number"
    elseif ctype == "bool" then
        return "boolean"
    elseif ctype == "char" or ctype == "std::string" then
        return "string"
    elseif ctype == "lua_State" then
        return "any"
    elseif ctype == "void" then
        return "nil"
    else
        return "any" -- 默认返回any
    end
end

-- 匹配文件中的所有include
local scanned_files = {}
local include_queue = {}
local cpp_content = {} -- filename -> content

local function add_to_queue(file)
    if scanned_files[file] then return end
    scanned_files[file] = true
    table.insert(include_queue, file)
end

-- 解析头文件，建立类和函数到注释/参数的映射字典
local symbol_dict = {}

local function parse_headers()
    -- 简易解析：只查找所有的 /** ... */ function(args) 以及 namespace::var = val
    for file, content in pairs(cpp_content) do
        -- 消除行注释但保留/* */
        local clean = string.gsub(content, "//.-\n", "\n")
        
        -- 按结构体或类寻找名字空间太难（不支持PCRE），可以直接按行匹配或者利用全局的正则
        -- 但我们需要精确一点
        -- 取出 /** ... */ 和接下来的函数
        for comment, func_sig in string.gmatch(content, "/%*%*(.-)%*/%s*([^;{]-%([^;{]-%)[^;{]-)[;{]") do
            local last_comment = string.match(comment, ".*%/%*%*(.*)")
            if last_comment then comment = last_comment end
            
            local before_args, args = string.match(func_sig, "^%s*(.-)%s*%((.*)%)")
            if before_args then
                local rettype, func_name = string.match(before_args, "^%s*(.-)%s*([%w_~]+)%s*$")
                if func_name then
                    rettype = string.gsub(rettype, "^%s*virtual%s+", "")
                    rettype = string.gsub(rettype, "^%s*static%s+", "")
                    if rettype == "" or string.match(rettype, "^%s+$") then rettype = "void" end
                    
                    table.insert(symbol_dict, {
                        comment = comment,
                        rettype = rettype,
                        name = func_name,
                        args = args,
                        file = file
                    })
                end
            end
        end
        
        -- 没有注释的函数也提取一下
        for func_sig in string.gmatch(clean, "\n%s*([^\n;{]-%([^;{]-%)[^;{]-)[;{]") do
            local before_args, args = string.match(func_sig, "^%s*(.-)%s*%((.*)%)")
            if before_args then
                local rettype, func_name = string.match(before_args, "^%s*(.-)%s*([%w_~]+)%s*$")
                if func_name then
                    rettype = string.gsub(rettype, "^%s*virtual%s+", "")
                    rettype = string.gsub(rettype, "^%s*static%s+", "")
                    if rettype == "" or string.match(rettype, "^%s+$") then rettype = "void" end
                    
                    table.insert(symbol_dict, {
                        comment = nil,
                        rettype = rettype,
                        name = func_name,
                        args = args,
                        file = file
                    })
                end
            end
        end
        
        -- 查找枚举或静态常量
        for enum_name, enum_val in string.gmatch(clean, "([%w_]+)%s*=%s*([^,;}]+)") do
            table.insert(symbol_dict, {
                name = enum_name,
                val = string.gsub(enum_val, "^%s*(.-)%s*$", "%1"),
                file = file
            })
        end
    end
end

local function find_symbol(cpp_symbol)
    local ns, name = split_namespace(cpp_symbol)
    local ns_lower = ns and string.lower(ns) or ""
    
    local best = nil
    local best_score = -1
    
    for i = #symbol_dict, 1, -1 do
        local sym = symbol_dict[i]
        if sym.name == name then
            local score = 0
            if sym.comment then score = score + 1 end
            if ns_lower ~= "" and sym.file then
                local f_clean = string.gsub(string.lower(sym.file), "_", "")
                local ns_clean = string.gsub(ns_lower, "_", "")
                if string.find(f_clean, ns_clean, 1, true) then
                    score = score + 2
                end
            end
            
            if score > best_score then
                best = sym
                best_score = score
            end
        end
    end
    return best
end

local function process_export(modules, classes)
    -- os.execute("mkdir " .. string.gsub(export_dir, "/", "\\") .. " 2>nul")
    
    for _, mod in ipairs(modules) do
        print("module", mod.lua_name)
        local out = {}
        local function write(str) table.insert(out, str) end
        write("---@diagnostic disable: missing-return\n")
        write(string.format("-- 导出模块: %s", mod.lua_name))
        write(string.format("%s = {}", mod.lua_name))
        write("")
        
        for _, func in ipairs(mod.funcs) do
            local sym = find_symbol(func.cpp_symbol)
            if not sym then
                print("Error: Symbol not found for " .. mod.lua_name .. "." .. func.lua_name .. " -> " .. func.cpp_symbol)
            else
                -- 转换注释
                local has_param = false
                local doc_params = {}
                if sym.comment then
                    for line in string.gmatch(sym.comment, "[^\r\n]+") do
                        line = string.gsub(line, "^%s*%*?\\?%s*", "")
                        if line ~= "" then
                            local pname = string.match(line, "^@param%s+([%w_]+)")
                            if pname then table.insert(doc_params, pname) end
                            
                            has_param = has_param or string.match(line, "^@param")
                            if string.match(line, "^@") then
                                write("---" .. line)
                            else
                                write("--- " .. line)
                            end
                        end
                    end
                end
                
                local args_str = ""
                local args_list = {}
                if sym.args and sym.args ~= "" and sym.args ~= "void" and sym.args ~= "lua_State *L" and sym.args ~= "lua_State* L" then
                    for arg in string.gmatch(sym.args, "([^,]+)") do
                        -- 忽略默认参数 = xxx
                        arg = string.gsub(arg, "%s*=.*", "")
                        
                        local aname = string.match(arg, "([%w_]+)%s*$")
                        local atype = "any"
                        if aname then
                            local raw_type = string.match(arg, "^%s*(.-)%s*" .. aname .. "%s*$")
                            if raw_type and raw_type ~= "" then
                                atype = raw_type
                            end
                        else
                            aname = string.match(arg, "([%w_]+)")
                        end
                        
                        if aname and aname ~= "L" then
                            table.insert(args_list, aname)
                            -- 如果没有 @param，或者在 @param 中没找到这个变量的说明，我们要自动加上
                            local matched_param = nil
                            if sym.comment and has_param then
                                matched_param = string.match(sym.comment, "@param%s+" .. aname .. "%s+")
                            end
                            
                            if not matched_param and not has_param then
                                print("Warning: Parameter " .. aname .. " missing comment in " .. mod.lua_name .. "." .. func.lua_name)
                                write("---@param " .. aname .. " " .. map_type(atype))
                            end
                        end
                    end
                else
                    if #doc_params > 0 then
                        for _, pname in ipairs(doc_params) do table.insert(args_list, pname) end
                    end
                end
                args_str = table.concat(args_list, ", ")
                
                write(string.format("function %s.%s(%s)", mod.lua_name, func.lua_name, args_str))
                write("end\n")
            end
        end
        
        local out_str = table.concat(out, "\n")
        local file_name = string.lower(mod.lua_name) .. ".lua"
        -- 以__前缀表示这个文件很特殊，不可引用。同时防止和业务文件重名
        local out_file = export_dir .. "__" .. file_name
        local f = io.open(out_file, "w")
        if f then
            f:write(out_str)
            f:close()
        end
    end
    
    for _, cls in ipairs(classes) do
        print("class", cls.lua_name )
        local out = {}
        local function write(str) table.insert(out, str) end
        write("---@diagnostic disable: missing-return\n")
        write(string.format("-- 导出类: %s", cls.lua_name))
        write(string.format("%s = {}", cls.lua_name))
        
        if #cls.vars > 0 then
            for _, v in ipairs(cls.vars) do
                local sym = find_symbol(v.cpp_symbol)
                local val = sym and sym.val or "0"
                if not sym then
                    print("Error: Variable not found for " .. cls.lua_name .. "." .. v.lua_name .. " -> " .. v.cpp_symbol)
                end
                write(string.format("%s.%s = %s", cls.lua_name, v.lua_name, val))
            end
            write("")
        end
        
        for _, func in ipairs(cls.methods) do
            local sym = find_symbol(func.cpp_symbol)
            if not sym then
                print("Error: Symbol not found for " .. cls.lua_name .. ":" .. func.lua_name .. " -> " .. func.cpp_symbol)
            else
                local has_param = false
                local doc_params = {}
                if sym.comment then
                    for line in string.gmatch(sym.comment, "[^\r\n]+") do
                        line = string.gsub(line, "^%s*%*?\\?%s*", "")
                        if line ~= "" then
                            local pname = string.match(line, "^@param%s+([%w_]+)")
                            if pname then table.insert(doc_params, pname) end
                            
                            has_param = has_param or string.match(line, "^@param")
                            if string.match(line, "^@") then
                                write("---" .. line)
                            else
                                write("--- " .. line)
                            end
                        end
                    end
                end
                
                local args_str = ""
                local args_list = {}
                if sym.args and sym.args ~= "" and sym.args ~= "void" and sym.args ~= "lua_State *L" and sym.args ~= "lua_State* L" then
                    for arg in string.gmatch(sym.args, "([^,]+)") do
                        -- 忽略默认参数 = xxx
                        arg = string.gsub(arg, "%s*=.*", "")
                        
                        local aname = string.match(arg, "([%w_]+)%s*$")
                        local atype = "any"
                        if aname then
                            local raw_type = string.match(arg, "^%s*(.-)%s*" .. aname .. "%s*$")
                            if raw_type and raw_type ~= "" then
                                atype = raw_type
                            end
                        else
                            aname = string.match(arg, "([%w_]+)")
                        end
                        
                        if aname and aname ~= "L" then
                            table.insert(args_list, aname)
                            
                            local matched_param = nil
                            if sym.comment and has_param then
                                matched_param = string.match(sym.comment, "@param%s+" .. aname .. "%s+")
                            end
                            
                            if not matched_param and not has_param then
                                print("Warning: Parameter " .. aname .. " missing comment in " .. cls.lua_name .. ":" .. func.lua_name)
                                write("---@param " .. aname .. " " .. map_type(atype))
                            end
                        end
                    end
                else
                    if #doc_params > 0 then
                        for _, pname in ipairs(doc_params) do table.insert(args_list, pname) end
                    end
                end
                args_str = table.concat(args_list, ", ")
                
                write(string.format("function %s:%s(%s)", cls.lua_name, func.lua_name, args_str))
                write("end\n")
            end
        end
        
        local out_str = table.concat(out, "\n")
        local file_name = string.lower(cls.lua_name) .. ".lua"
        -- 以__前缀表示这个文件很特殊，不可引用。同时防止和业务文件重名
        local out_file = export_dir .. "__" .. file_name
        local f = io.open(out_file, "w")
        if f then
            f:write(out_str)
            f:close()
        end
    end
end

-- 开始解析
local function main()
    local beg = os.clock()
    
    local main_content = read_file(entrance)
    if not main_content then
        print("Failed to open " .. entrance)
        return
    end
    
    -- 提取include并递归读取
    add_to_queue(entrance)
    local head = 1
    while head <= #include_queue do
        local f = include_queue[head]
        head = head + 1
        
        local c = read_file(f)
        if c then
            cpp_content[f] = c
            local cur_dir = string.match(f, "^(.*/)[^/]+$") or ""
            -- 寻找头文件里的 #include "xxx"
            for inc in string.gmatch(c, '#include%s+"([^"]+)"') do
                -- 构建新路径
                local new_path = cur_dir .. inc
                local test_f = io.open(new_path, "r")
                if test_f then
                    test_f:close()
                else
                    new_path = dir .. inc
                end
                add_to_queue(new_path)
            end
        end
    end
    
    -- 解析类和模块
    local modules = {}
    local classes = {}
    
    local c = main_content
    local current_module = nil
    
    for line in string.gmatch(c, "[^\r\n]+") do
        -- module
        local mod_name = string.match(line, 'lcpp::module_begin%(L,%s*"([^"]+)"%)')
        if mod_name then
            current_module = { lua_name = mod_name, funcs = {} }
            table.insert(modules, current_module)
        end
        local func_cpp, func_lua = string.match(line, 'lcpp::module_function<&([^>]+)>%(L,%s*"([^"]+)"%)')
        if func_cpp and current_module then
            table.insert(current_module.funcs, { cpp_symbol = func_cpp, lua_name = func_lua })
        end
        if string.match(line, 'lcpp::module_end%(L%)') then
            current_module = nil
        end
        
        -- class
        local cls_lua = string.match(line, 'lcpp::Class<[^>]+>%s+%w+%(L,%s*"([^"]+)"%)')
        if cls_lua then
            -- 去掉engine.
            cls_lua = string.gsub(cls_lua, "^engine%.", "")
            current_module = { lua_name = cls_lua, methods = {}, vars = {} }
            table.insert(classes, current_module)
        end
        local m_cpp, m_lua = string.match(line, 'lc%.def<&([^>]+)>%("([^"]+)"%)')
        if not m_cpp then
            m_cpp, m_lua = string.match(line, 'lc%.def_pointer_call<&([^>]+)>%("([^"]+)"%)')
        end
        if m_cpp and current_module then
            table.insert(current_module.methods, { cpp_symbol = m_cpp, lua_name = m_lua })
        end
        
        -- lc.set(Packet::PT_HTTP, "PT_HTTP");
        local v_cpp, v_lua = string.match(line, 'lc%.set%(([^,]+),%s*"([^"]+)"%)')
        if v_cpp and current_module then
            table.insert(current_module.vars, { cpp_symbol = v_cpp, lua_name = v_lua })
        end
    end
    
    parse_headers()
    
    process_export(modules, classes)
    
    print("export done, time " .. os.clock() - beg)
end

main()
