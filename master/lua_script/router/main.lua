require "global.global"
require "global.oo"
require "global.table"

local client_mgr = require "net.client_mgr"
local Sql = require "Sql"
require "signal.signal"

local function main()
    client_mgr:listen( "0.0.0.0",9997 )

    local sql = Sql();
    
    oo.check()
    ev:run()
end

xpcall( main, __G__TRACKBACK__ )
