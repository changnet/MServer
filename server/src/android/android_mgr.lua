-- 机器人管理

local Android = require "android.android"

local AndroidMgr = oo.singleton( ... )

function AndroidMgr:__init()
    self.conn    = {}
    self.android = {}
end

function AndroidMgr:set_android_conn(conn_id,android)
    if android then assert(nil == self.conn[conn_id]) end

    self.conn[conn_id] = android
end

function AndroidMgr:start()
    local srvindex = tonumber(g_app.srvindex) -- 平台
    local srvid = tonumber(g_app.srvid) -- 服务器
    for index = 1,srvid do
        local idx = ( srvindex << 16 ) | index
        self.android[idx] = Android( idx )
    end
end

function AndroidMgr:get_android_by_conn(conn_id)
    return self.conn[conn_id]
end

function AndroidMgr:routine( ms_now )
    for _,android in pairs(self.android) do
        android:routine( ms_now )
    end
end

local android_mgr = AndroidMgr()

return android_mgr

