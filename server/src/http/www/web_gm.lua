-- web_gm.lua http gm接口
local WebGM = oo.singleton(...)

-- 通过web方式执行gm
function WebGM:exec(conn, fields, body)
    -- GET 方式 http://127.0.0.1:10003/web_gm?gm=@gh
    -- curl POST
    -- @-表示从stdin读入数据，curl本来有个--data-raw参数的，但是很多版本用不了
    -- echo @ghf | curl -l -H "Content-type: text/plain" --data '@-' 127.0.0.1:10003/web_gm
    local gm = body or fields.gm
    if not gm then return HTTP.INVALID, body end

    local ok, msg = GM.exec("web_gm", nil, gm)
    if not ok then return HTTP.INVALID, msg or gm end

    return HTTP.OK, msg
end

local wgm = WebGM()

return wgm
