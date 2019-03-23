-- 机器人管理

local Android = require "android.android"

local Android_mgr = oo.singleton( nil,... )

function Android_mgr:__init()
    self.conn    = {}
    self.android = {}
end

function Android_mgr:set_android_conn(conn_id,android)
    if android then assert(nil == self.conn[conn_id]) end

    self.conn[conn_id] = android
end

function Android_mgr:start()
    local srvindex = tonumber(g_app.srvindex) -- 平台
    local srvid = tonumber(g_app.srvid) -- 服务器
    for index = 1,1 do
        local idx = ( srvid << 16 ) | index
        self.android[idx] = Android( idx )
    end
end

function Android_mgr:get_android_by_conn(conn_id)
    return self.conn[conn_id]
end

function Android_mgr:routine( ms_now )
    for _,android in pairs(self.android) do
        android:routine( ms_now )
    end
end

local android_mgr = Android_mgr()

return android_mgr


