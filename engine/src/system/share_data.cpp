#include "share_data.hpp"
#include <lua.hpp>
#include <stdexcept>
#include <sstream>
#include <iomanip>

/**
 * @brief 自定义异常
 */
class ShareDataException : public std::runtime_error
{
public:
    ShareDataException(const std::string &msg, const ShareData::Key &last_key)
        : std::runtime_error(msg), last_key_(last_key), has_key_(true)
    {
    }

    ShareDataException(const std::string &msg)
        : std::runtime_error(msg), has_key_(false)
    {
    }

    void prepend_path(const std::string &p)
    {
        if (path_.empty())
            path_ = p;
        else
            path_ = p + (p.back() == ']' ? "" : ".") + path_;
    }

    const std::string &get_path() const
    {
        return path_;
    }

private:
    std::string path_;
    ShareData::Key last_key_;
    bool has_key_;
};

// --- Node 实现 ---

ShareData::Node::~Node()
{
    clear();
}

void ShareData::Node::clear()
{
    if (std::holds_alternative<Table>(value_))
    {
        auto &map = std::get<Table>(value_);
        for (auto &pair : map)
        {
            delete pair.second;
        }
        map.clear();
    }
    value_ = std::monostate{};
}

size_t ShareData::Node::calc_memory_size() const
{
    size_t size = sizeof(Node);
    std::visit(
        [&size](auto &&arg)
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::string>)
            {
                size += arg.capacity();
            }
            else if constexpr (std::is_same_v<T, Table>)
            {
                size += sizeof(Table);
                for (const auto &[key, node] : arg)
                {
                    std::visit(
                        [&size](auto &&k_arg)
                        {
                            using KT = std::decay_t<decltype(k_arg)>;
                            if constexpr (std::is_same_v<KT, std::string>)
                            {
                                size += k_arg.capacity();
                            }
                        },
                        key);
                    size += node->calc_memory_size();
                }
            }
        },
        value_);
    return size;
}

// --- ShareData 实现 ---

ShareData::ShareData()
{
    root_node_         = new Node();
    root_node_->value_ = Table{};
}

ShareData::~ShareData()
{
    delete root_node_;
}

std::string ShareData::key_to_string(const Key &k)
{
    return std::visit(
        [](auto &&arg) -> std::string
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, int64_t>)
            {
                return "[" + std::to_string(arg) + "]";
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                return arg;
            }
            else if constexpr (std::is_same_v<T, double>)
            {
                std::ostringstream oss;
                oss << std::setprecision(15) << "[" << arg << "]";
                return oss.str();
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                return arg ? "[true]" : "[false]";
            }
            return "unknown";
        },
        k);
}

bool ShareData::try_get_key(lua_State *L, int idx, Key &key)
{
    int type = lua_type(L, idx);
    if (type == LUA_TNUMBER)
    {
        if (lua_isinteger(L, idx))
            key = (int64_t)lua_tointeger(L, idx);
        else
            key = (double)lua_tonumber(L, idx);
        return true;
    }
    else if (type == LUA_TSTRING)
    {
        // C++ 20可以用const char*构建一个map的透明对比key，避免频繁创建std::string
        static_assert(__cplusplus < 202002L);
        key = std::string(lua_tostring(L, idx));
        return true;
    }
    else if (type == LUA_TBOOLEAN)
    {
        key = (bool)lua_toboolean(L, idx);
        return true;
    }
    return false;
}

void ShareData::set_internal(Node *root, lua_State *L, int index, int depth,
                             bool is_update)
{
    if (depth > 64)
    {
        throw ShareDataException("ShareData set depth limit exceeded (64)");
    }

    int type = lua_type(L, index);
    if (type == LUA_TNIL)
    {
        root->clear();
    }
    else if (type == LUA_TBOOLEAN)
    {
        root->clear();
        root->value_ = (bool)lua_toboolean(L, index);
    }
    else if (type == LUA_TNUMBER)
    {
        root->clear();
        if (lua_isinteger(L, index))
            root->value_ = (int64_t)lua_tointeger(L, index);
        else
            root->value_ = (double)lua_tonumber(L, index);
    }
    else if (type == LUA_TSTRING)
    {
        root->clear();
        root->value_ = std::string(lua_tostring(L, index));
    }
    else if (type == LUA_TTABLE)
    {
        if (!is_update || !std::holds_alternative<Table>(root->value_))
        {
            root->clear();
            root->value_ = Table{};
        }

        auto &map = std::get<Table>(root->value_);
        lua_pushnil(L);
        while (lua_next(L, index) != 0)
        {
            Key key;
            if (!try_get_key(L, -2, key))
            {
                lua_pop(L, 1);
                throw ShareDataException(
                    "ShareData table key unsupported type: "
                    + std::string(luaL_typename(L, -2)));
            }

            Node *next = nullptr;
            auto it    = map.find(key);
            if (it == map.end())
            {
                next = new Node();
                // Move key if possible, though here it's copy unless we are
                // careful. map[Key] moves if we passed rvalue? Not exactly with
                // operator[]. Use emplace for efficiency
                map.emplace(key, next);
            }
            else
            {
                next = it->second;
            }

            try
            {
                set_internal(next, L, lua_gettop(L), depth + 1, is_update);
            }
            catch (ShareDataException &e)
            {
                e.prepend_path(key_to_string(key));
                lua_pop(L, 2);
                throw;
            }
            catch (...)
            {
                lua_pop(L, 2);
                throw;
            }
            lua_pop(L, 1);
        }
    }
    else
    {
        throw ShareDataException(std::string("ShareData unsupported type: ")
                                 + lua_typename(L, type));
    }
}

void ShareData::push_node(lua_State *L, const Node *node)
{
    std::visit(
        [this, L](auto &&arg)
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>)
                lua_pushnil(L);
            else if constexpr (std::is_same_v<T, bool>)
                lua_pushboolean(L, arg);
            else if constexpr (std::is_same_v<T, int64_t>)
                lua_pushinteger(L, arg);
            else if constexpr (std::is_same_v<T, double>)
                lua_pushnumber(L, arg);
            else if constexpr (std::is_same_v<T, std::string>)
                lua_pushlstring(L, arg.c_str(), arg.size());
            else if constexpr (std::is_same_v<T, Table>)
            {
                lua_newtable(L);
                for (const auto &[key, child] : arg)
                {
                    push_key(L, key);
                    push_node(L, child);
                    lua_settable(L, -3);
                }
            }
        },
        node->value_);
}

void ShareData::push_key(lua_State *L, const Key &key)
{
    std::visit(
        [L](auto &&arg)
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, int64_t>)
                lua_pushinteger(L, arg);
            else if constexpr (std::is_same_v<T, std::string>)
                lua_pushstring(L, arg.c_str());
            else if constexpr (std::is_same_v<T, double>)
                lua_pushnumber(L, arg);
            else if constexpr (std::is_same_v<T, bool>)
                lua_pushboolean(L, arg);
        },
        key);
}

int ShareData::set_kv(const char *k, const char *v)
{
    Key key = std::string(k);

    std::unique_lock lock(mutex_);

    auto &map = std::get<Table>(root_node_->value_);
    auto it   = map.find(key);

    Node *node;
    if (it == map.end())
    {
        node = new Node();
        map.emplace(std::move(key), node);
    }
    else
    {
        node = it->second;
    }
    node->value_ = std::string(v);

    return 0;
}

int ShareData::set(lua_State *L)
{
    int top = lua_gettop(L);
    if (top < 3) return 0;

    try
    {
        std::unique_lock lock(mutex_);
        Node *curr = root_node_;

        for (int i = 2; i < top; ++i)
        {
            Key key;
            if (!try_get_key(L, i, key))
            {
                lua_pushboolean(L, false);
                lua_pushstring(L,
                               "Key must be number, string, double or boolean");
                return 2;
            }

            if (!std::holds_alternative<Table>(curr->value_))
            {
                curr->clear();
                curr->value_ = Table{};
            }

            auto &map = std::get<Table>(curr->value_);
            auto it   = map.find(key);
            if (it == map.end())
            {
                Node *next = new Node();
                // Efficiently insert using move semantics
                map.emplace(std::move(key), next);
                curr = next;
            }
            else
            {
                curr = it->second;
            }
        }

        try
        {
            set_internal(curr, L, top, 0, false);
        }
        catch (ShareDataException &e)
        {
            for (int i = top - 1; i >= 2; --i)
            {
                Key k;
                try_get_key(L, i, k);
                e.prepend_path(key_to_string(k));
            }
            throw;
        }

        lua_pushboolean(L, true);
        return 1;
    }
    catch (const ShareDataException &e)
    {
        lua_pushboolean(L, false);
        lua_pushfstring(L, "%s at %s", e.what(), e.get_path().c_str());
        return 2;
    }
    catch (const std::exception &e)
    {
        lua_pushboolean(L, false);
        lua_pushstring(L, e.what());
        return 2;
    }
}

int ShareData::unset(lua_State *L)
{
    int top = lua_gettop(L);
    if (top < 2) return 0;

    std::unique_lock lock(mutex_);
    Node *curr = root_node_;

    for (int i = 2; i < top; ++i)
    {
        Key key;
        if (!try_get_key(L, i, key))
        {
            lua_pushboolean(L, false);
            lua_pushstring(L, "Key must be number, string, double or boolean");
            return 2;
        }

        if (!std::holds_alternative<Table>(curr->value_))
        {
            // 如果中间路径不是 table，说明要移除的东西本来就不存在
            lua_pushboolean(L, true);
            return 1;
        }

        auto &map = std::get<Table>(curr->value_);
        auto it   = map.find(key);
        if (it == map.end())
        {
            // 如果中间路径不存在，说明要移除的东西本来就不存在
            lua_pushboolean(L, true);
            return 1;
        }
        else
        {
            curr = it->second;
        }
    }

    // 处理最后一个 key
    Key last_key;
    if (!try_get_key(L, top, last_key))
    {
        lua_pushboolean(L, false);
        lua_pushstring(L, "Key must be number, string, double or boolean");
        return 2;
    }

    if (std::holds_alternative<Table>(curr->value_))
    {
        auto &map = std::get<Table>(curr->value_);
        auto it   = map.find(last_key);
        if (it != map.end())
        {
            delete it->second;
            map.erase(it);
        }
    }

    lua_pushboolean(L, true);
    return 1;
}

int ShareData::get(lua_State *L)
{
    int top = lua_gettop(L);

    std::shared_lock lock(mutex_);
    Node *curr = root_node_;
    for (int i = 2; i <= top; ++i)
    {
        Key key;
        if (!try_get_key(L, i, key)) return 0;

        if (!std::holds_alternative<Table>(curr->value_)) return 0;

        const auto &map = std::get<Table>(curr->value_);
        auto it         = map.find(key);
        if (it == map.end()) return 0;

        curr = it->second;
    }

    push_node(L, curr);
    return 1;
}

int ShareData::fetch_into(lua_State *L)
{
    int top = lua_gettop(L);
    if (top < 4) return 0;

    if (!lua_istable(L, top)) return 0;     // cache table
    if (!lua_istable(L, top - 1)) return 0; // fields table

    std::shared_lock lock(mutex_);
    Node *curr = root_node_;
    for (int i = 2; i <= top - 2; ++i)
    {
        Key key;
        if (!try_get_key(L, i, key)
            || !std::holds_alternative<Table>(curr->value_))
        {
            return 0;
        }

        const auto &map = std::get<Table>(curr->value_);
        auto it         = map.find(key);
        if (it == map.end()) return 0;

        curr = it->second;
    }

    if (!std::holds_alternative<Table>(curr->value_)) return 0;

    const auto &map = std::get<Table>(curr->value_);

    lua_pushnil(L);
    while (lua_next(L, top - 1) != 0)
    {
        Key field_key;
        if (try_get_key(L, -2, field_key))
        {
            auto it = map.find(field_key);
            if (it != map.end())
            {
                lua_pushvalue(L, -2); // field key
                push_node(L, it->second);
                lua_settable(L, top);
            }
        }
        lua_pop(L, 1);
    }

    lua_pushboolean(L, true);
    return 1;
}

int ShareData::fetch_add(lua_State *L)
{
    int top = lua_gettop(L);
    if (top < 3) return 0;

    int64_t add_val = luaL_checkinteger(L, top);

    std::unique_lock lock(mutex_);
    Node *curr = root_node_;
    for (int i = 2; i < top; ++i)
    {
        Key key;
        if (!try_get_key(L, i, key)) return 0;
        if (!std::holds_alternative<Table>(curr->value_)) return 0;

        auto &map = std::get<Table>(curr->value_);
        auto it   = map.find(key);
        if (it == map.end()) return 0;

        curr = it->second;
    }

    if (!std::holds_alternative<int64_t>(curr->value_)) return 0;

    int64_t old_val = std::get<int64_t>(curr->value_);
    curr->value_    = old_val + add_val;

    lua_pushinteger(L, old_val);
    return 1;
}

int ShareData::keys(lua_State *L)
{
    int top       = lua_gettop(L);
    int cache_idx = 0;
    int end_idx   = top;

    if (top >= 2 && lua_istable(L, top))
    {
        cache_idx = top;
        end_idx   = top - 1;
    }

    std::shared_lock lock(mutex_);
    Node *curr = root_node_;
    for (int i = 2; i <= end_idx; ++i)
    {
        Key key;
        if (!try_get_key(L, i, key)
            || !std::holds_alternative<Table>(curr->value_))
        {
            lua_pushnil(L);
            return 1;
        }
        const auto &map = std::get<Table>(curr->value_);
        auto it         = map.find(key);
        if (it == map.end())
        {
            lua_pushnil(L);
            return 1;
        }
        curr = it->second;
    }

    if (!std::holds_alternative<Table>(curr->value_)) return 0;

    if (cache_idx == 0)
    {
        lua_newtable(L);
        cache_idx = lua_gettop(L);
    }
    else
    {
        lua_pushvalue(L, cache_idx);
        cache_idx = lua_gettop(L);
    }

    const auto &map = std::get<Table>(curr->value_);
    int n           = 0;
    for (const auto &[key, _] : map)
    {
        lua_pushinteger(L, ++n);
        push_key(L, key);

        lua_settable(L, cache_idx);
    }

    lua_pushstring(L, "n");
    lua_pushinteger(L, n);
    lua_settable(L, cache_idx);

    return 1;
}

int ShareData::update(lua_State *L)
{
    int top = lua_gettop(L);
    if (top < 3) return 0;

    try
    {
        std::unique_lock lock(mutex_);
        Node *curr = root_node_;

        for (int i = 2; i < top; ++i)
        {
            Key key;
            if (!try_get_key(L, i, key))
            {
                lua_pushboolean(L, false);
                lua_pushstring(L,
                               "Key must be number, string, double or boolean");
                return 2;
            }

            if (!std::holds_alternative<Table>(curr->value_))
            {
                curr->clear();
                curr->value_ = Table{};
            }

            auto &map = std::get<Table>(curr->value_);
            auto it   = map.find(key);
            if (it == map.end())
            {
                Node *next = new Node();
                map.emplace(std::move(key), next);
                curr = next;
            }
            else
            {
                curr = it->second;
            }
        }

        try
        {
            set_internal(curr, L, top, 0, true);
        }
        catch (ShareDataException &e)
        {
            for (int i = top - 1; i >= 2; --i)
            {
                Key key;
                try_get_key(L, i, key);
                e.prepend_path(key_to_string(key));
            }
            throw;
        }

        lua_pushboolean(L, true);
        return 1;
    }
    catch (const ShareDataException &e)
    {
        lua_pushboolean(L, false);
        lua_pushfstring(L, "%s at %s", e.what(), e.get_path().c_str());
        return 2;
    }
    catch (const std::exception &e)
    {
        lua_pushboolean(L, false);
        lua_pushstring(L, e.what());
        return 2;
    }
}

int ShareData::memory_size(lua_State *L)
{
    std::shared_lock lock(mutex_);
    size_t size = root_node_->calc_memory_size();
    lua_pushinteger(L, (lua_Integer)size);
    return 1;
}
