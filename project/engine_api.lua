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

-- 查找当前位置所在的最后声明的class/struct/namespace
local function get_scope(content, pos)
    local sub = string.sub(content, 1, pos)
    local last_ns = ""
    -- 匹配 class/struct/namespace Name ... { 或者 :
    -- 考虑分号、括号等干扰，只在全局或作用域开始处寻找
    for p, kw, name, sep in string.gmatch(sub, "()(%w+)%s+([%w_]+)[%s%w_:<>,]*([{:])") do
        if kw == "class" or kw == "struct" or kw == "namespace" then
            local body = string.sub(sub, p + #kw + #name + 1)
            -- 我们需要数在这之后、当前 pos 之前有多少个大括号对
            local _, opens = string.gsub(body, "{", "{")
            local _, closes = string.gsub(body, "}", "}")
            -- 如果 opens >= closes，说明我们在该作用域内
            -- 注意：gsub(body, "{", "{") 会包含匹配 sep 时可能匹配到的那个 { 
            if opens > closes or (sep == ":" and not string.find(body, "{", 1, true)) then
                last_ns = name
            end
        end
    end
    return last_ns
end

local function parse_headers()
    -- 简易解析：只查找所有的 /** ... */ function(args) 以及 namespace::var = val
    for file, content in pairs(cpp_content) do
        -- 消除行注释但保留/* */
        local clean = string.gsub(content, "//.-\n", "\n")
        
        -- 取出 /** ... */ 和接下来的函数
        for pos, comment, func_sig in string.gmatch(content, "()/%*%*(.-)%*/%s*([^;{]-%([^;{]-%)[^;{]-)[;{]") do
            local last_comment = string.match(comment, ".*%/%*%*(.*)")
            if last_comment then comment = last_comment end
            
            local before_args, args = string.match(func_sig, "^%s*(.-)%s*%((.*)%)")
            if before_args then
                local rettype, func_name = string.match(before_args, "^%s*(.-)%s*([%w_:~]+)%s*$")
                if func_name and not string.match(func_name, "^(if|while|for|switch|catch)$") then
                    local explicit_ns = ""
                    if string.find(func_name, "::") then
                        explicit_ns, func_name = string.match(func_name, "^(.*)::(.*)$")
                    end
                    local ns = explicit_ns ~= "" and explicit_ns or get_scope(content, pos)
                    
                    rettype = string.gsub(rettype, "^%s*virtual%s+", "")
                    rettype = string.gsub(rettype, "^%s*static%s+", "")
                    if rettype == "" or string.match(rettype, "^%s+$") then rettype = "void" end
                    
                    table.insert(symbol_dict, {
                        ns = ns,
                        comment = comment,
                        rettype = rettype,
                        name = func_name,
                        args = args,
                        file = file,
                        is_header = string.match(file, "%.[hH][pP]?[pP]?$") ~= nil
                    })
                end
            end
        end
        
        -- 没有注释的函数也提取一下
        -- 优化正则表达式，排除掉 ctx->xxx(...) 这样的调用，并要求有返回类型（至少两个词且其中有空格）
        for pos, func_sig in string.gmatch(clean, "()\n%s*([^\n;{%(%.]+%s+[^%s%.%->%(]+%([^;{]-%)[^;{]-)[;{]") do
            local before_args, args = string.match(func_sig, "^%s*(.-)%s*%((.*)%)")
            if before_args then
                local rettype, func_name = string.match(before_args, "^%s*(.-)%s*([%w_:~]+)%s*$")
                -- 再次过滤掉含有赋值和指针操作的非声明行
                if func_name and not string.match(func_name, "^(if|while|for|switch|catch)$") 
                   and not string.find(before_args, "->", 1, true)
                   and not string.find(before_args, "=", 1, true)
                   and not string.find(before_args, ".", 1, true) then
                    local explicit_ns = ""
                    if string.find(func_name, "::") then
                        explicit_ns, func_name = string.match(func_name, "^(.*)::(.*)$")
                    end
                    local ns = explicit_ns ~= "" and explicit_ns or get_scope(clean, pos)
                    
                    rettype = string.gsub(rettype, "^%s*virtual%s+", "")
                    rettype = string.gsub(rettype, "^%s*static%s+", "")
                    if rettype == "" or string.match(rettype, "^%s+$") then rettype = "void" end
                    
                    table.insert(symbol_dict, {
                        ns = ns,
                        comment = nil,
                        rettype = rettype,
                        name = func_name,
                        args = args,
                        file = file,
                        is_header = string.match(file, "%.[hH][pP]?[pP]?$") ~= nil
                    })
                end
            end
        end
        
        -- 查找枚举或静态常量
        for pos, enum_name, enum_val in string.gmatch(clean, "()([%w_]+)%s*=%s*([^,;}]+)") do
            local ns = get_scope(clean, pos)
            table.insert(symbol_dict, {
                ns = ns,
                name = enum_name,
                val = string.gsub(enum_val, "^%s*(.-)%s*$", "%1"),
                file = file
            })
        end
    end
end

local function find_symbol(cpp_symbol)
    local ns, name = split_namespace(cpp_symbol)
    local ns_lower = string.lower(ns)
    
    local best = nil
    local best_score = -1
    local best_commented = nil
    local comment_score = -1
    
    for i = 1, #symbol_dict do
        local sym = symbol_dict[i]
        if sym.name == name then
            local score = 0
            
            -- 基本分：头文件 1000，注释 500
            if sym.is_header then score = score + 1000 end
            if sym.comment then score = score + 500 end
            
            if ns_lower ~= "" then
                local sym_ns_lower = sym.ns and string.lower(sym.ns) or ""
                if sym_ns_lower == ns_lower then
                    score = score + 10000 -- 名字空间完全匹配，极高优先级
                else
                    -- 不同名字空间干扰惩罚：如果是显式请求 A 却匹配到非空的 B，大幅降分
                    if sym_ns_lower ~= "" then
                        score = score - 8000
                    end
                    
                    if sym.file then
                        local f_clean = string.gsub(string.lower(sym.file), "[_&*]", "")
                        local ns_clean = string.gsub(string.lower(ns_lower), "[_&*]", "")
                        if string.byte(ns_clean, 1) == string.byte("l", 1) then
                            ns_clean = string.sub(ns_clean, 2)
                        end
                        if string.find(f_clean, ns_clean, 1, true) then
                            score = score + 200
                        end
                    end
                end
            else
                -- 没有指定名字空间的情况（全局函数），偏向头文件且名字空间也为空的
                if not sym.ns or sym.ns == "" then
                    score = score + 500
                end
            end
            
            if score > best_score then
                best = sym
                best_score = score
            end
            
            -- 同时寻找带注释的最优项
            if sym.comment then
                if score > comment_score then
                    best_commented = sym
                    comment_score = score
                end
            end
        end
    end
    
    -- 智能合并：如果最准确的匹配项没有注释，但我们找到了同名的带注释项
    if best and not best.comment and best_commented then
        local b_ns = best.ns and string.lower(best.ns) or ""
        local c_ns = best_commented.ns and string.lower(best_commented.ns) or ""
        
        -- 严格的合并条件：
        -- 1. 名字空间完全一致
        -- 2. 或者它们就在同一个源文件中 (非常重要，解决 GridAoi 首个函数丢失问题)
        -- 3. 或者其中一个是名字空间 A，另一个是 LA
        local match = (b_ns ~= "" and b_ns == c_ns) or (best.file == best_commented.file)
        
        if not match then
            if b_ns ~= "" and c_ns ~= "" then
                local b_clean = string.gsub(b_ns, "^l", "")
                local c_clean = string.gsub(c_ns, "^l", "")
                if b_clean == c_clean then match = true end
            elseif b_ns == "" or c_ns == "" then
                -- 如果其中一个为空，且文件名高度相似（比如 lgrid_aoi.cpp 匹配 lgrid_aoi.hpp）
                local f1 = string.gsub(string.lower(best.file or ""), "%.%a+$", "")
                local f2 = string.gsub(string.lower(best_commented.file or ""), "%.%a+$", "")
                if f1 ~= "" and f1 == f2 then match = true end
            end
        end
        
        -- 增加：绝对禁止跨模块（如 Mongo vs timing）合并
        -- 如果两者都有名字空间，且名字空间经过上面校验仍不匹配，则禁止
        if not match then
             -- Do nothing
        else
            -- 克隆 best 并把注释合过来
            local cloned = {}
            for k,v in pairs(best) do cloned[k] = v end
            cloned.comment = best_commented.comment
            return cloned
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
            
            -- 处理 include
            -- 支持 #include "xxx" 和 #include <xxx>
            for quote, inc in string.gmatch(c, '#include%s+([%s<"])([^>%s"]+)[>%s"]') do
                local paths = {
                    cur_dir .. inc,           -- 相对当前目录
                    dir .. inc,               -- 引擎源码目录
                }
                
                -- 如果是 deps 目录下的子目录，比如 #include <lparson.h> 而它在 lua_parson/lparson.h
                -- 这里尝试在 deps 下递归查找或者常见位置
                if string.match(inc, "parson") then table.insert(paths, deps_dir .. "lua_parson/" .. inc) end
                if string.match(inc, "rapidxml") then table.insert(paths, deps_dir .. "lua_rapidxml/" .. inc) end
                
                for _, path in ipairs(paths) do
                    local test_f = io.open(path, "r")
                    if test_f then
                        test_f:close()
                        add_to_queue(path)
                        
                        -- 如果是头文件，尝试寻找对应的实现文件
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
    
    -- 解析 LUA_LIB_OPEN
    for ns, mod_name, func_name in string.gmatch(c, 'LUA_LIB_OPEN%("([%w_]+)%.([^"]+)",%s*([%w_]+)%)') do
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
    
    parse_headers()
    
    process_export(modules, classes)
    
    print("export done, time " .. os.clock() - beg)
end

main()
