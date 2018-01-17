-- 机器人指令处理入口

local sc = require "proto.proto"
SC,CS = sc[1],sc[2]

local android_mgr = require "android.android_mgr"

android_mgr:init_sc_command( SC )
android_mgr:init_cs_command( CS )

local Android = require "android.android"

android_mgr:cmd_register( SC.PLAYER_LOGIN,Android.on_login )
android_mgr:cmd_register( SC.PLAYER_PING,Android.on_ping )
android_mgr:cmd_register( SC.PLAYER_CREATE,Android.on_create_role )
android_mgr:cmd_register( SC.PLAYER_ENTER,Android.on_enter_world )
android_mgr:cmd_register( SC.PLAYER_OTHER,Android.on_login_otherwhere )