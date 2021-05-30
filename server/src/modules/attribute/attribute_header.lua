-- attribute_header.lua
-- xzc
-- 2018-12-01
-- 战斗属性宏定义
-- 属性类型定义 attribute type
ABT = {
    NONE = 0, -- 无效
    HP = 1, -- 血量
    MAX_HP = 2, -- 最大血量
    ATK = 3, -- 攻击力
    DEF = 4, -- 防御
    MOVE = 5, -- 移动速度，按像素计算

    -- 具体属性在以上添加,记得修改以下最大值
    MAX = 5
}

-- 属性系统定义
ABTSYS = {
    NONE = 0, -- 无效
    SYNC = 1, -- world中的玩家属性同步到area，放到一个系统中
    BASE = 2, -- 等级基础属性

    -- 具体系统在以上添加,记得修改以下最大值
    MAX = 3
}

return ABT
