-- 具体的路由规则实现

local hashstr = Util.hashstr
local hashint = Util.hashint

-- 使用哈希一致性分配一个
local function hash(size, value)
    if 0 == size then error("no size for hash") end

    local t = type(value)
    if "string" == t then
        -- key="accouht" value = 帐号
        return (hashstr(value) % size) + 1
    elseif "number" == t then
        -- key="pid" value=pid
        return (hashint(value) % size) + 1
    else
        error("invalid type for hash: ", t)
    end
end

local function hash_factory(wtype)
    return function(key, value)
        local list = Router.get_worker_list(wtype)

        local size = #list
        if 1 == size then
            return list[1].addr
        end

        local index = hash(size, value)
        return list[index].addr
    end
end

Router.set_policy(W.MONGODB, hash_factory(W.MONGODB))
Router.set_policy(W.ACCOUNT, hash_factory(W.ACCOUNT))
Router.set_policy(W.MYSQL, hash_factory(W.MYSQL))
