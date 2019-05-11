-- web_gm.lua http gm接口

local Web_gm = oo.singleton( nil,... )

--[[
-- @-表示从stdin读入数据，curl本来有个--data-raw参数的，但是很多版本用不了
echo ghf | curl -l -H "Content-type: application/json" --data '@-' 127.0.0.1:10003/Web_gm
]]
function Web_gm:exec( fields,body )
    if not body or not g_gm:exec( "web_gm",nil,body ) then
        return HTTPE.GM_INVALID,body
    end

    return HTTPE.OK
end

local wgm = Web_gm()

return wgm
