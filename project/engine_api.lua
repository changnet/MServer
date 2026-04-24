-- 导出引擎中的接口，用于代码提示和luacheck

-- 在project目录执行 lua engine_api.lua


-- 入口文件，所有导出的符号都是从这里开始查找的
local entrance = "../engine/src/lpp/llib.cpp"

-- C++源码目录，要导出的符号肯定在这个目录的源码中
local dir = "../engine/src/"
-- 外部依赖目录
local deps_dir = "../engine/deps/"

-- 导出目录
local export_dir = "../server/engine/"

local function space_replacer(match)
    return string.gsub(match, "[^\n\r]", " ")
end

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
    -- 去掉 & 和 * 等前缀
    cpp_symbol = string.gsub(cpp_symbol, "^[&*]+", "")
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

local scanned_files = {}
local include_queue = {}
local cpp_content = {} -- filename -> content

local function add_to_queue(file)
    if scanned_files[file] then return end
    scanned_files[file] = true
    table.insert(include_queue, file)
end

----------------------- 1. 解析 INCLUDE -----------------------

local function parse_includes(main_file)
    local main_content = read_file(main_file)
    if not main_content then
        print("Failed to open " .. main_file)
        return nil
    end
    
    add_to_queue(main_file)
    local head = 1
    while head <= #include_queue do
        local f = include_queue[head]
        head = head + 1
        
        local c = read_file(f)
        if c then
            cpp_content[f] = c
            local cur_dir = string.match(f, "^(.*/)[^/]+$") or ""
            
            for quote, inc in string.gmatch(c, '#include%s+([%s<"])([^>%s"]+)[>%s"]') do
                local paths = {
                    cur_dir .. inc,
                    dir .. inc,
                }
                
                if string.match(inc, "parson") then table.insert(paths, deps_dir .. "lua_parson/" .. inc) end
                if string.match(inc, "rapidxml") then table.insert(paths, deps_dir .. "lua_rapidxml/" .. inc) end
                
                for _, path in ipairs(paths) do
                    local test_f = io.open(path, "r")
                    if test_f then
                        test_f:close()
                        add_to_queue(path)
                        
                        local base = string.match(path, "^(.*)%.[hH][pP]?[pP]?$")
                        if base then
                            for _, ext in ipairs({".cpp", ".c"}) do
                                local src_path = base .. ext
                                local sf = io.open(src_path, "r")
                                if sf then
                                    sf:close()
                                    add_to_queue(src_path)
                                end
                            end
                        end
                        break
                    end
                end
            end
        end
    end
    return main_content
end

----------------------- 2. 解析 类和模块定义 -----------------------

local function parse_modules_and_classes(main_content)
    local modules = {}
    local classes = {}
    local current_module = nil
    
    for line in string.gmatch(main_content, "[^\r\n]+") do
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
        local cls_cpp, cls_lua = string.match(line, 'lcpp::Class<([^>]+)>%s+%w+%(L,%s*"([^"]+)"%)')
        if cls_lua then
            cls_lua = string.gsub(cls_lua, "^engine%.", "")
            current_module = { lua_name = cls_lua, cpp_name = cls_cpp, methods = {}, vars = {} }
            table.insert(classes, current_module)
        end
        local m_cpp, m_lua = string.match(line, 'lc%.def<&([^>]+)>%("([^"]+)"%)')
        if not m_cpp then
            m_cpp, m_lua = string.match(line, 'lc%.def_pointer_call<&([^>]+)>%("([^"]+)"%)')
        end
        if m_cpp and current_module then
            table.insert(current_module.methods, { cpp_symbol = m_cpp, lua_name = m_lua })
        end
        
        local v_cpp, v_lua = string.match(line, 'lc%.set%(([^,]+),%s*"([^"]+)"%)')
        if v_cpp and current_module then
            table.insert(current_module.vars, { cpp_symbol = v_cpp, lua_name = v_lua })
        end
    end
    
    return modules, classes
end

----------------------- 3. 解析 LUA_LIB_OPEN -----------------------

local function parse_lua_lib_open(main_content, modules)
    for ns, mod_name, func_name in string.gmatch(main_content, 'LUA_LIB_OPEN%("([%w_]+)%.([^"]+)",%s*([%w_]+)%)') do
        local reg_array = nil
        for f_path, content in pairs(cpp_content) do
            local func_body = string.match(content, func_name .. "%s*%(.-%).-%{(.-)%}")
            if func_body then
                reg_array = string.match(func_body, "luaL_newlib%([^,]+,%s*([%w_]+)%s*%)")
                if not reg_array then
                    reg_array = string.match(func_body, "luaL_setfuncs%([^,]+,%s*([%w_]+)%s*,")
                end
                
                if reg_array then
                    local array_def = string.match(content, reg_array .. "%s*%[%]%s*=%s*%{(.-)%}%s*;")
                    if array_def then
                        local mod = { lua_name = mod_name, funcs = {} }
                        for lua_fn, cpp_fn in string.gmatch(array_def, '{%s*"([^"]+)"%s*,%s*([%w_:~]+)%s*}') do
                            if lua_fn ~= "" and cpp_fn ~= "nullptr" and cpp_fn ~= "NULL" then
                                table.insert(mod.funcs, { cpp_symbol = cpp_fn, lua_name = lua_fn })
                            end
                        end
                        if #mod.funcs > 0 then
                            table.insert(modules, mod)
                        end
                    end
                end
                break
            end
        end
    end
end


----------------------- 4. 符号查找逻辑 -----------------------

local function extract_comments(content, func_pos)
    local before = string.sub(content, 1, func_pos - 1)
    
    local lines = {}
    local s = 1
    while true do
        local e = string.find(before, "\n", s)
        if not e then
            table.insert(lines, string.sub(before, s))
            break
        end
        table.insert(lines, string.sub(before, s, e - 1))
        s = e + 1
    end
    
    local cs = {}
    for i = #lines, 1, -1 do
        local line = string.gsub(lines[i], "\r", "")
        if string.match(line, "^%s*$") then
            if #cs > 0 then break end
        else
            local lcmt = string.match(line, "^%s*//+[%s]*(.*)")
            if lcmt then
                table.insert(cs, 1, lcmt)
            else
                if string.match(line, "%*/%s*$") then
                    local bcmt = string.match(line, "^%s*%*+(.*)%*/%s*$") or string.match(line, "^%s*(.*)%*/%s*$")
                    if string.match(line, "/%*") then
                        bcmt = string.match(line, "/%*%*?[%s]*(.*)%*/%s*$")
                    end
                    if bcmt and string.match(bcmt, "%S") then table.insert(cs, 1, bcmt) end
                elseif string.match(line, "^%s*/%*%*?[%s]*(.*)") then
                    local bcmt = string.match(line, "^%s*/%*%*?[%s]*(.*)")
                    if bcmt and string.match(bcmt, "%S") then table.insert(cs, 1, bcmt) end
                elseif string.match(line, "^%s*%*[%s]*(.*)") then
                    local bcmt = string.match(line, "^%s*%*[%s]*(.*)")
                    table.insert(cs, 1, bcmt)
                else
                    break
                end
            end
        end
    end
    
    if #cs > 0 then return table.concat(cs, "\n") end
    return nil
end

local function search_method_in_file(content, method_name)
    local _, bare_method = split_namespace(method_name)
    local clean = string.gsub(content, "/%*.-%*/", space_replacer)
    clean = string.gsub(clean, "//[^\n]*", space_replacer)
    
    for pos, rettype, func_name, args in string.gmatch(clean, "()\n[ \t]*([^\n;{%(%.]-[^ \t\n;{%(%.])[ \t]+([^ \t\n%.%->%(]+)[ \t]*%(([^;{]-)%)[^;{]-[;{]") do
        if func_name and not string.match(func_name, "^(if|while|for|switch|catch)$") then
            local _, bare_func = split_namespace(func_name)
            if bare_func == bare_method then
                rettype = string.gsub(rettype, "^%s*virtual%s+", "")
                rettype = string.gsub(rettype, "^%s*static%s+", "")
                if rettype == "" or string.match(rettype, "^%s+$") then rettype = "void" end
                
                local comment = extract_comments(content, pos)
                return { rettype = rettype, args = args, comment = comment, name = bare_method }
            end
        end
    end
    
    return nil
end

local function find_class_declaration(class_name)
    local _, bare_class = split_namespace(class_name)
    for file, content in pairs(cpp_content) do
        for bchar, rest, term in string.gmatch(content, "class%s+" .. bare_class .. "([%s:{}])([^;{}%*%(%)]-)([;{])") do
            if bchar == "{" or term == "{" then
                local bases = {}
                local full_decl = (bchar == "{" or bchar == "}") and "" or (bchar .. rest)
                if string.find(full_decl, ":") then
                    for base in string.gmatch(full_decl, "public%s+([%w_:]+)") do
                        local _, bare_base = split_namespace(base)
                        table.insert(bases, bare_base)
                    end
                end
                return file, content, bases
            end
        end
        for bchar, rest, term in string.gmatch(content, "struct%s+" .. bare_class .. "([%s:{}])([^;{}%*%(%)]-)([;{])") do
            if bchar == "{" or term == "{" then
                local bases = {}
                local full_decl = (bchar == "{" or bchar == "}") and "" or (bchar .. rest)
                if string.find(full_decl, ":") then
                    for base in string.gmatch(full_decl, "public%s+([%w_:]+)") do
                        local _, bare_base = split_namespace(base)
                        table.insert(bases, bare_base)
                    end
                end
                return file, content, bases
            end
        end
    end
    return nil, nil, {}
end

local function search_method(class_name, method_name, visited)
    visited = visited or {}
    local _, bare_class = split_namespace(class_name)
    if visited[bare_class] then return nil end
    visited[bare_class] = true

    local file, content, bases = find_class_declaration(bare_class)
    if not file then return nil end

    local res = search_method_in_file(content, method_name)
    if res then
        res.file = file
        return res
    end
    
    for _, base in ipairs(bases) do
        res = search_method(base, method_name, visited)
        if res then return res end
    end
    
    return nil
end

local function search_global_function(func_name)
    local _, bare_func = split_namespace(func_name)
    for file, content in pairs(cpp_content) do
        local res = search_method_in_file(content, bare_func)
        if res then
            res.file = file
            return res
        end
    end
    return nil
end

local function search_global_variable(var_name)
    local _, bare_var = split_namespace(var_name)
    for file, content in pairs(cpp_content) do
        local clean = string.gsub(content, "//.-\n", "\n")
        for pos, enum_name, enum_val in string.gmatch(clean, "()([%w_]+)%s*=%s*([^,;}]+)") do
            if enum_name == bare_var then
                return {
                    name = enum_name,
                    val = string.gsub(enum_val, "^%s*(.-)%s*$", "%1"),
                    file = file
                }
            end
        end
    end
    return nil
end

local function resolve_symbols(modules, classes)
    for _, mod in ipairs(modules) do
        for _, func in ipairs(mod.funcs) do
            local sym = search_global_function(func.cpp_symbol)
            if not sym then
                print("Error: Global Function not found for " .. mod.lua_name .. "." .. func.lua_name .. " -> " .. func.cpp_symbol)
            else
                func.sym = sym
            end
        end
    end
    
    for _, cls in ipairs(classes) do
        for _, v in ipairs(cls.vars) do
            local sym = search_global_variable(v.cpp_symbol)
            if not sym then
                print("Error: Variable not found for " .. cls.lua_name .. "." .. v.lua_name .. " -> " .. v.cpp_symbol)
            else
                v.sym = sym
            end
        end
        for _, func in ipairs(cls.methods) do
            local class_name, method_name = split_namespace(func.cpp_symbol)
            if class_name == "" and cls.cpp_name then
                class_name = cls.cpp_name
            end
            
            local sym = search_method(class_name, method_name)
            if not sym then
                print("Error: Method not found for " .. cls.lua_name .. ":" .. func.lua_name .. " -> " .. func.cpp_symbol)
            else
                func.sym = sym
            end
        end
    end
end

----------------------- 5. 格式化和写入导出文件 -----------------------

local function generate_function_lua(sym, mod_name, is_method, func_lua_name, write_func)
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
                    write_func("---" .. line)
                else
                    write_func("--- " .. line)
                end
            end
        end
    end
    
    local args_str = ""
    local args_list = {}
    if sym.args and sym.args ~= "" and sym.args ~= "void" and sym.args ~= "lua_State *L" and sym.args ~= "lua_State* L" then
        for arg in string.gmatch(sym.args, "([^,]+)") do
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
                    print("Warning: Parameter " .. aname .. " missing comment in " .. mod_name .. (is_method and ":" or ".") .. func_lua_name)
                    write_func("---@param " .. aname .. " " .. map_type(atype))
                end
            end
        end
    else
        if #doc_params > 0 then
            for _, pname in ipairs(doc_params) do table.insert(args_list, pname) end
        end
    end
    args_str = table.concat(args_list, ", ")
    
    if is_method then
        write_func(string.format("function %s:%s(%s)", mod_name, func_lua_name, args_str))
    else
        write_func(string.format("function %s.%s(%s)", mod_name, func_lua_name, args_str))
    end
    write_func("end\n")
end

local function write_exports(modules, classes)
    for _, mod in ipairs(modules) do
        print("module", mod.lua_name)
        local out = {}
        local function write(str) table.insert(out, str) end
        write("---@diagnostic disable: missing-return\n")
        write(string.format("-- 导出模块: %s", mod.lua_name))
        write(string.format("%s = {}", mod.lua_name))
        write("\n")
        
        for _, func in ipairs(mod.funcs) do
            if func.sym then
                generate_function_lua(func.sym, mod.lua_name, false, func.lua_name, write)
            else
                write(string.format("function %s.%s(...)", mod.lua_name, func.lua_name))
                write("end\n")
            end
        end
        
        local out_str = table.concat(out, "\n")
        local file_name = string.lower(mod.lua_name) .. ".lua"
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
                local val = v.sym and v.sym.val or "0"
                write(string.format("%s.%s = %s", cls.lua_name, v.lua_name, val))
            end
            write("")
        end
        
        for _, func in ipairs(cls.methods) do
            if func.sym then
                generate_function_lua(func.sym, cls.lua_name, true, func.lua_name, write)
            else
                write(string.format("function %s:%s(...)", cls.lua_name, func.lua_name))
                write("end\n")
            end
        end
        
        local out_str = table.concat(out, "\n")
        local file_name = string.lower(cls.lua_name) .. ".lua"
        local out_file = export_dir .. "__" .. file_name
        local f = io.open(out_file, "w")
        if f then
            f:write(out_str)
            f:close()
        end
    end
end

local function main()
    local beg = os.clock()
    
    -- 1. 解析 INCLUDE，加载所有关联C++内容
    local main_content = parse_includes(entrance)
    if not main_content then return end
    
    -- 2. 解析 类和模块定义
    local modules, classes = parse_modules_and_classes(main_content)
    
    -- 3. 解析 LUA_LIB_OPEN
    parse_lua_lib_open(main_content, modules)
    
    -- 4. 去所有文件中搜索符号和注释
    resolve_symbols(modules, classes)
    
    -- 5. 将解析好的数据统一写入文件
    write_exports(modules, classes)
    
    print("export done, time " .. os.clock() - beg)
end

main()
