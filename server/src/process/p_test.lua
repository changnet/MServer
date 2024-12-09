-- 测试进程入口文件(此文件不热更)

PROCESS_ID  = PROCESS_TEST
g_env:set("process_id", PROCESS_ID)

LOCAL_ADDR = Engine.make_address(PROCESS_ID, 0, 1)

require "message.thread_message"
require "engine.signal"

print("tttttttttttttttttttttttttttttttttest", g_env:get("process_id"))
