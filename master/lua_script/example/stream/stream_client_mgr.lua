-- stream_client_mgr.lua
-- 2016-04-24
-- xzc

local Stream_client_connection = require "example.stream.stream_client_connection"
local Stream_client_mgr = oo.singleton( nil,... )

function Stream_client_mgr:__init()
    self.clts = {}
end

function Stream_client_mgr:start( ip,port,cnt )
    for i = 1,cnt do
        local clt = Stream_client_connection()
        local fd = clt:connect( ip,port )

        self.clts[fd] = clt
    end
end

function Stream_client_mgr:remove_connection( fd )
    self.clts[fd] = nil
end

return Stream_client_mgr
