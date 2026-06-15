#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <climits>

struct lua_State;
struct lua_Debug;

/**
 * @brief Lua函数级性能分析器
 *
 * 使用lua_sethook记录每个函数的调用和返回时间，
 * 支持协程环境，输出带调用栈层级的性能报告。
 * Lua侧创建对象并持有，每个线程同一时刻只能有一个活跃的实例。
 */
class Profile
{
public:
    Profile();
    ~Profile();

    // Lua接口，签名匹配int(T::*)(lua_State*)，通过fun_thunk调用
    // 开始hook，min_t为过滤阈值（毫秒）
    int begin_hook(lua_State *L);
    // 结束hook，把报告写入path指定的文件
    int end_hook(lua_State *L);
    // 给协程添加/解除hook: set_hook(co, true/false)
    int set_hook(lua_State *L);

private:
    // 调用树节点
    struct Node
    {
        std::string key;         // "source:line:name"
        int64_t total_time = 0;  // 总耗时（毫秒）
        int64_t times = 0;       // 调用次数
        int64_t min_time = INT64_MAX;
        int64_t max_time = 0;

        // 子节点，保持插入顺序
        std::vector<Node *> children_vec;
        std::unordered_map<std::string, Node *> children_map;

        ~Node();
        // 获取或创建子节点
        Node *get_or_create(const std::string &k);
    };

    // 调用栈帧
    struct Frame
    {
        Node *node;           // 对应的树节点
        int64_t enter_time;   // 进入时间（毫秒）
    };

    // 协程状态
    struct CoState
    {
        std::vector<Frame> stack;    // 调用栈
        int64_t suspend_time = 0;   // 挂起时间点
    };

    // hook回调（静态，通过tl_active_访问实例）
    static void hook_cb(lua_State *L, lua_Debug *ar);
    void on_call(lua_State *L, lua_Debug *ar, bool tail);
    void on_return(lua_State *L);

    // 记录函数耗时到节点
    void record_frame(Frame &frame, int64_t now);

    // 输出报告
    void write_report(const char *path);
    void write_node(FILE *fp, Node *node, int depth);

    // 清理所有数据
    void reset();

    bool hooking_ = false;
    int32_t min_time_ = 0;     // 过滤阈值（毫秒）
    Node root_;                // 调用树根节点（虚拟）
    std::unordered_map<lua_State *, CoState> stacks_;

    // 当前线程活跃的Profile实例
    static thread_local Profile *tl_active_;
};
