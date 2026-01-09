#include <shared_mutex>
#include <unordered_map>
#include <variant>
#include <vector>
#include <string>
#include "global/global.hpp"

struct lua_State;

/**
 * @brief 实现多个线程共享数据，类似一个简单的内存redis
 */
class ShareData
{
public:
    // Key可以是数字、字符串、双精度浮点数、布尔值
    using Key = std::variant<int64_t, std::string, double, bool>;

    struct KeyHash
    {
        std::size_t operator()(const Key &k) const
        {
            return std::visit(
                [](auto &&arg) -> std::size_t
                { return std::hash<std::decay_t<decltype(arg)>>{}(arg); },
                k);
        }
    };

    ShareData();
    ~ShareData();

    // 禁止拷贝
    ShareData(const ShareData &)            = delete;
    ShareData &operator=(const ShareData &) = delete;

    /**
     * @brief 设置数据
     * sharedata:set("user_list", 12345, {name = "abc", level = 123})
     * @return 成功返回 true，失败返回 false, err_msg
     */
    int set(lua_State *L);

    /**
     * @brief 移除数据
     * sharedata:unset("user_list", 12345)
     * @return 成功返回 true
     */
    int unset(lua_State *L);

    /**
     * @brief 获取数据
     * sharedata:get("user_list", 12345)
     */
    int get(lua_State *L);

    /**
     * @brief 获取多个字段到cache中
     * sharedata:fetch_into("user_list", 12345, {name = 1, level = 1}, cache)
     */
    int fetch_into(lua_State *L);

    /**
     * @brief 原子加法，只支持对整数操作
     * sharedata:fetch_add("user_list", 12345, "level", 1)
     */
    int fetch_add(lua_State *L);

    /**
     * @brief 获取所有key
     * sharedata:keys("user_list")
     */
    int keys(lua_State *L);

    /**
     * @brief 合并更新数据
     * sharedata:update("user_list", 12345, {name = "abc"})
     * @return 成功返回 true，失败返回 false, err_msg
     */
    int update(lua_State *L);

    /**
     * @brief 获取当前对象占用的内存大小
     */
    int memory_size(lua_State *L);

private:
    struct Node;
    using Table = std::unordered_map<Key, Node *, KeyHash>;

    // Node存储实际数据
    struct Node
    {
        enum class Type
        {
            NIL,
            BOOLEAN,
            INTEGER,
            NUMBER,
            STRING,
            TABLE
        };

        Type type_ = Type::NIL;
        std::variant<std::monostate, bool, int64_t, double, std::string, Table> value_;

        Node() = default;
        ~Node();

        // 计算当前节点及其子节点的内存大小
        size_t calc_memory_size() const;

        // 深度清理
        void clear();
    };

    /**
     * @brief 内部递归设置/更新数据
     * @param root 当前处理的节点
     * @param L lua状态机
     * @param index table在lua栈中的索引
     * @param depth 当前递归深度
     * @param is_update 是否为合并更新
     */
    void set_internal(Node *root, lua_State *L, int index, int depth,
                      bool is_update);

    /**
     * @brief 内部递归将数据压入lua栈
     */
    void push_node(lua_State *L, const Node *node);
    /**
     * @brief 内部递归将key压入lua栈
     */
    void push_key(lua_State *L, const Key &key);

    /**
     * @brief 将Key转为字符串，用于报错信息
     */
    static std::string key_to_string(const Key &k);

    /**
     * @brief 从Lua栈解析一个Key
     */
    static bool try_get_key(lua_State *L, int idx, Key &key);

private:
    Node *root_node_;
    mutable std::shared_mutex mutex_;
};
