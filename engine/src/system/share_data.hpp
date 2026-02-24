#include <shared_mutex>
#include <unordered_map>
#include <variant>
#include <vector>
#include <string>
#include "global/global.hpp"

struct lua_State;

/**
 * 实现这个模块的初衷，是让多个lua虚拟机共享数据。例如：把玩家基础数据存在这里，则
 * 所有lua虚拟机都可以访问，不需要通过rpc调用。
 * 
 * 但是要理解这个共享的数据是线程安全，但不具备一致性。比如你正在for循环查找某个
 * 等级的玩家，查询玩家A的等级后，另一个线程就修改了B的等级，这时候A、B的状态是不
 * 处于遍历开始时的状态的。
 * 
 * 所以不能用它来存配置
 * 
 * 另外，这个模块读写数据并不快，写比lua慢了5倍左右，内存多占50%，读慢了60倍，即使
 * 用fetch模式，也慢了30倍，具体的看share_data_test.lua
 * 
 * 但是用这个来交互数据，是要比rpc调用快，并且节省很多的
 */

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
     * @brief 用于在C++简单设置数据。暂时没有复杂需求，因此不支持多级key
     */
    int set_kv(const char *k, const char *v);

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
        // 移除 Type type_，直接使用 index 判断类型
        // 索引对应关系：
        // 0: std::monostate (NIL)
        // 1: bool (BOOLEAN)
        // 2: int64_t (INTEGER)
        // 3: double (NUMBER)
        // 4: std::string (STRING)
        // 5: Table (TABLE)
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
     * @brief 从Lua栈解析一个Key (产生 string 拷贝)
     */
    static bool try_get_key(lua_State *L, int idx, Key &key);

private:
    Node *root_node_;
    mutable std::shared_mutex mutex_;
};
