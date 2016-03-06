
local Json = require "lua_parson"

function var_dump(data, max_level, prefix)
    if type(prefix) ~= "string" then
        prefix = ""
    end

    if type(data) ~= "table" then
        print(prefix .. tostring(data))
    else
        print(data)
        if max_level ~= 0 then
            local prefix_next = prefix .. "    "
            print(prefix .. "{")
            for k,v in pairs(data) do
                io.stdout:write(prefix_next .. k .. " = ")
                if type(v) ~= "table" or (type(max_level) == "number" and max_level <= 1) then
                    print(v)
                else
                    if max_level == nil then
                        var_dump(v, nil, prefix_next)
                    else
                        var_dump(v, max_level - 1, prefix_next)
                    end
                end
            end
            print(prefix .. "}")
        end
    end
end

--[[
    eg: local b = {aaa="aaa",bbb="bbb",ccc="ccc"}
]]
function vd(data, max_level)
    var_dump(data, max_level or 20)
end


local function set_array( tb,flag )
    local _tb = tb or {}
    local mt = getmetatable( _tb ) or {}
    rawset( mt,"__array",flag )
    setmetatable( _tb,mt )

    return _tb
end

local test_data = {}

test_data.employees =
{
    { firstName = "Bill" , lastName = "Gates" },
    { firstName = "George" , lastName = "Bush" },
    { firstName = "Thomas" , lastName = "Carter" }
}

test_data.sparse = { [10] = "number ten" }
test_data.empty_array = set_array( {},true )
test_data.empty_object = set_array( {},false )
test_data.force_array  = set_array( { phone1 = "123456789",phone2 = "987654321" },true )
test_data.force_object = set_array( { "USA","UK","CH" },false )

local json_str = Json.encode( test_data )  -- Json.encode( test_data,true )
print( json_str )

-- local result = Json.encode_to_file( test_data,"test.json",true )
-- local decode_result = Json.decode_from_file("test.json")

local decode_result = Json.decode( json_str )
vd( decode_result )

local t =
{
    [1] = "kdjdddjdjd"
}

vd( t )
