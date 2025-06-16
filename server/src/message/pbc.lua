-- pbc的的protobuf在lua的接口
Pbc = {}

local PbcCodec = require "engine.PbcCodec"

local g_pbc = _G.g_pbc or PbcCodec()
local pbc_decode = g_pbc.decode

local function load_file(path)
    local f = io.open(path, "rb") -- 必须以binary模式打开
    local buffer = f:read("a")
    f:close()

    return g_pbc:load(buffer)
end

-- 加载pb文件
function Pbc.load()
    local tm = Engine.steady_clock()
    g_pbc:reset()

    local count = 0
    local srv_dir = g_env:get("srv_dir")

    -- 注意：pbc中如果一个pb文件引用了另一个pb文件中的message，则另一个文件必须优先加载
    local priority =
    {
        "/pb/comm.pb",
    }
    local loaded = {}
    for _, path in pairs(priority) do
        path = srv_dir .. path

        if not load_file(path) then
            printf("fail to load %s", path)
            return false
        end

        count = count + 1
        loaded[path] = true
    end

    local files = Util.ls(srv_dir .. "/pb")
    for _, path in pairs(files or {}) do
        if not loaded[path] and string.end_with(path, "pb") then
            if not load_file(path) then
                printf("fail to load %s", path)
                return false
            end

            count = count + 1
        end
    end

    g_pbc:update()
    printf("pbc load %d scehma files, time %d ms",
        count, Engine.steady_clock() - tm)
end

function Pbc.update()
    return g_pbc:update()
end

function Pbc.decode(schema, buffer, size)
    return pbc_decode(schema, buffer, size)
end
