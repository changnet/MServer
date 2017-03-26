-- 机器人指令处理入口

local sc = require "command/sc_command"
SC,CS = sc[1],sc[2]

local android_mgr = require "android/android_mgr"

local Android = require "android/android"

android_mgr:cmd_register( SC.PLAYER_LOGIN,Android.on_login )
android_mgr:cmd_register( SC.PLAYER_PING,Android.on_ping )