-- ai管理

-- ai名字以aiN来命名，方便策划配置怪物AI逻辑
-- ai的加载通过ai_mgr的load函数来加载
-- ai调用比较频繁，但比较少改动。是否有必要使用class?

-- ai通用接口
-- __init
-- execute
-- 

local Ai_mgr = oo.singleton( nil,... )

function Ai_mgr:__init()
end

function AI_mgr:load( entity,index )
end