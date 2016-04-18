-- http_client_mgr.lua
-- 2016-04-18
-- xzc

local Http_client_mgr = oo.singleton( nil,... )
local Http_client_connection = require "example.http.http_client_connection"

function Http_client_mgr:__init()
    self.clts = {}
end

function Http_client_mgr:start( cnt,ip,port )
    for i = 1,cnt do
        local clt = Http_client_connection()
        local fd = clt:connect( ip,port )
        self.clts[fd] = clt
    end

    self.sec,self.usec = util.timeofday()
end

function Http_client_mgr:remove_connection( fd )
    self.clts[fd] = nil

    if table.empty( self.clts ) then
        local sec,usec = util.timeofday()

        print( "http client test done,cost",
        math.floor( (sec-self.sec)*1000000 + usec - self.usec ),
            "microsecond" )
    end
end

return Http_client_mgr
