-- web_gm.lua http gm接口

local Web_gm = oo.singleton( nil,... )

--[[
-- 本来gm格式是@ghf这要的，但curl占用了@这个特殊符号，发不出去
curl -l -H "Content-type: application/json" -X POST -d 'ghf' 127.0.0.1:10003/Web_gm
]]
function Web_gm:exec( fields,body )
    if not body or not g_gm:exec( "web_gm",nil,body ) then
        return HTTPE.GM_INVALID
    end

    return HTTPE.OK
end

local wgm = Web_gm()

return wgm
