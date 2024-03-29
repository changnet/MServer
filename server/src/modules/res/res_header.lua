-- 资源模块宏定义

--[[
资源格式{ 1, 999, bind = 0 }
1. 第一个数字表示id
    1. [1, 99] 单一种类虚拟资源，如 元宝、铜钱等，共支持99种
    2. [100, 599] 多种类的虚拟资源，共支持59种，每种占10，共支持10个子类
        例如某个系统需要五行之力，则类型为100，子类型为100-104分别表示金木水火土
        下一种资源则必须从110开始
        如果后续扩展超过10（比如五行改成12生肖），只能通过占用另一个段来处理，
        id就不是连续的了，对应的逻辑特殊处理即可，这种情况也极少见
    3. [600, 999] 为不需要存库，实时处理的虚拟资源，如 增加时间，减少cd等特殊奖励
        这类资源需要对应的模块注册一个处理函数
    4. >= 1000的为道具，道具id需要在道具表配置
2. 第二个数字表示数量
3. 其他字段，根据需求添加
    1. unbind = 1 非绑定，仅对道具生效，默认为绑定
    2. attr = {} 随机属性
    3. star = 9 星级
    4. strong = 9 强化等级
4. 每种资源都要在下面定义，方便维护
5. 虚拟资源，是指在游戏中发放、使用，但又不占用背包的资源，简称货币
    目前都在玩家基础模块统一处理，只要定义类型即可
    当然，如果有特殊需求，也可以在对应的模块自己处理这种资源的发放及存库
]]

RES_GOLD   = 1    -- 元宝
RES_COPPER = 2    -- 铜钱
RES_ITEM   = 1000 -- >=此值的均为道具
