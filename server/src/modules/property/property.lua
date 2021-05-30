-- property.lua
-- xzc
-- 2018-12-07
-- 玩家属性定义
PROP = {
    -- {field,db_save,scene_sync,client_sync,scene_broadcast}
    -- @field:字段名，在player的base中以哪个字段保存数据
    -- @db_save,是否存库
    -- @scene_sync:是否同步到场景，一些在场景中显示的数据(比如名字、帮派名)要同步
    -- @client_sync:是否同步到前端，前端要显示的就同步
    -- @scene_broadcast:是否场景广播，别人能看到的，就广播。比如别人的名字变了，视野中的人都会收到变更
    -- 这些属性大多数是int类型，也有部分是double、string，会通过统一的机制进行异步同步到
    {"name", true, true, true}, -- 名字
    {"level", true, true, true}, -- 等级
    {"gold", true, false, true, false} -- 元宝数
}

return PROP
