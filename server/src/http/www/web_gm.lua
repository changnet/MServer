-- web_gm.lua http gm接口
local WebGM = oo.singleton(...)

--[[
-- @-表示从stdin读入数据，curl本来有个--data-raw参数的，但是很多版本用不了
echo ghf | curl -l -H "Content-type: application/json" --data '@-' 127.0.0.1:10003/web_gm
]]
function WebGM:exec(conn, fields, body)
    if not body then return HTTP.INVALID, body end

    local ok, msg = g_gm:exec("web_gm", nil, body)
    if not ok then return HTTP.INVALID, msg or body end

    return HTTP.OK, msg
end

local wgm = WebGM()

return wgm
