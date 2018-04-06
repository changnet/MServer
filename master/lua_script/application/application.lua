-- application.lua
-- 2018-04-04
-- xzc

-- app进程基类

require "modules.system.define"

local Application = oo.class( nil,... )

-- 信号处理，默认情况下退出
function sig_handler( signum )
    g_app:shutdown()
    ev:exit()
end

-- 初始化
function Application:__init( ... )
    self.init_list = {} -- 初始化列表
    self.command,self.srvname,self.srvindex,self.srvid = ...

    self.starttime = ev:time()
    self.session = self:srv_session(
        self.srvname,tonumber(self.srvindex),tonumber(self.srvid) )

    -- 设置当前session到C++
    network_mgr:set_curr_session( self.session )
end

-- 生成服务器session id
-- @name  服务器名称，如gateway、world...
-- @index 服务器索引，如多个gateway，分别为1,2...
-- @srvid 服务器id，与运维相关。开了第N个服
function Application:srv_session( name,index,srvid )
    local ty = SRV_NAME[name]

    assert( ty,"server name type not define" )
    assert( index < (1 << 24),"server index out of boundry" )
    assert( srvid < (1 << 16),   "server id out of boundry" )

    -- int32 ,8bits is ty,8bits is index,16bits is srvid
    return (ty << 24) + (index <<16) + srvid
end

-- 解析session id
function Application:srv_session_parse( session )
    local ty = session >> 24;
    local index = (session >> 16) & 0xFF
    local srvid = session & 0xFFFF

    return ty,index,srvid
end

-- 关服处理
function Application:shutdown()
end

-- 加载各个子模块
function Application:module_initialize()
    require "modules.module_header"
end

-- 设置初始化后续动作
-- val：数量，比如说可能需要等待多个场景服务器连接
function Application:set_initialize( name,after,action,val )
    self.init_list[name] = {after = after,action = action,val = val or 1 }
end

-- 一个初始化完成
function Application:one_initialized( name,val )
    local init = self.init_list[name]
    if not init then
        return ELOG( "unknow initialize action:%s",name )
    end

    init.val = init.val - (val or 1)
    if init.val <= 0 then
        self.init_list[name] = nil
        PLOG( "%s initialize OK",name )

        for _,init in pairs( self.init_list ) do
            if init.after == name then init.action( self ) end
        end
    end

    if table.empty( self.init_list ) then self:final_initialize() end
end

-- 进程初始化
function Application:initialize()
    g_command_mgr:load_schema()
    for _,init in pairs( self.init_list ) do
        if not init.after and init.action then init.action( self ) end
    end
end

-- 初始化完成
function Application:final_initialize()
    self.ok = true
    PLOG( "%s server(0x%.8X) start OK",self.srvname,self.session )
end

-- 运行进程
function Application:exec()
    ev:signal( 2  )
    ev:signal( 15 )

    self:initialize()

    ev:backend()
end

-- 连接db
function Application:db_initialize()
    local callback = function()
        self:one_initialized( "db_conn" )
    end
    -- 连接数据库
    g_mongodb:start( g_setting.mongo_ip,
        g_setting.mongo_port,g_setting.mongo_user,
        g_setting.mongo_pwd,g_setting.mongo_db,callback )
end

-- 加载自增id
function Application:uniqueid_initialize()
    g_unique_id:db_load()
end

return Application
