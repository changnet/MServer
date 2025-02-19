-- worker通用逻辑
Worker = {}

WorkerHash = {} -- 以worker的addr为key，worker对象为value
WorkerSetting = {} -- 以worker的addr为ke，worker的配置为value

LOCAL_ADDR = 0 -- 当前worker地址
LOCAL_TYPE = 0 -- 当前worker类型

-- 获取worker的名字，如gateway1
function Worker.name(addr)
end
