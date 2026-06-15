#include "profile.hpp"

#include <cstdio>
#include <algorithm>

#include <lua.hpp>

#include "ev/time.hpp"
#include "global/global.hpp"

thread_local Profile *Profile::tl_active_ = nullptr;

// ==================== Node ====================

Profile::Node::~Node()
{
    for (auto *child : children_vec)
    {
        delete child;
    }
}

Profile::Node *Profile::Node::get_or_create(
    const std::string &k)
{
    auto it = children_map.find(k);
    if (it != children_map.end()) return it->second;

    Node *node = new Node();
    node->key = k;
    children_map[k] = node;
    children_vec.push_back(node);
    return node;
}

// ==================== Profile ====================

Profile::Profile()
{
}

Profile::~Profile()
{
    if (tl_active_ == this)
    {
        tl_active_ = nullptr;
    }
    // root_析构会自动释放子节点
}

// 记录一个栈帧的耗时到对应的树节点
void Profile::record_frame(Frame &frame)
{
    int64_t duration = timing::steady_clock() - frame.enter_time;

    frame.node->times++;
    frame.node->total_time += duration;
    if (duration < frame.node->min_time)
    {
        frame.node->min_time = duration;
    }
    if (duration > frame.node->max_time)
    {
        frame.node->max_time = duration;
    }
}

// ==================== Hook回调 ====================

void Profile::hook_cb(lua_State *L, lua_Debug *ar)
{
    if (!tl_active_) return;

    if (ar->event == LUA_HOOKCALL
        || ar->event == LUA_HOOKTAILCALL)
    {
        tl_active_->on_call(
            L, ar, ar->event == LUA_HOOKTAILCALL);
    }
    else if (ar->event == LUA_HOOKRET)
    {
        tl_active_->on_return(L);
    }
}

void Profile::on_call(
    lua_State *L, lua_Debug *ar, bool tail)
{
    lua_getinfo(L, "Sn", ar);

    // 构建key: source:line:name
    char buf[256];
    snprintf(buf, sizeof(buf), "%s:%d:%s",
             ar->short_src,
             ar->linedefined,
             ar->name ? ar->name : "?");

    std::string key(buf);
    auto &cs = stacks_[L];

    // 尾调用：先结算上一个栈帧
    if (tail && !cs.stack.empty())
    {
        record_frame(cs.stack.back());
        cs.stack.pop_back();
    }

    // 找到父节点，获取或创建子节点
    Node *parent = cs.stack.empty()
                       ? &root_
                       : cs.stack.back().node;

    Node *node = parent->get_or_create(key);
    cs.stack.push_back({node, timing::steady_clock()});
}

void Profile::on_return(lua_State *L)
{
    // begin_hook肯定是在一个函数中调用，因此这个函数是记录不到开始，不配对的
    auto it = stacks_.find(L);
    if (it == stacks_.end() || it->second.stack.empty()) return;

    auto &cs = it->second;
    record_frame(cs.stack.back());
    cs.stack.pop_back();

    // 调用栈为空时，从map删除避免协程GC后泄漏
    if (cs.stack.empty())
    {
        stacks_.erase(it);
    }
}

// ==================== Lua接口 ====================

// Lua: profiler:begin_hook(min_t)
int Profile::begin_hook(lua_State *L)
{
    int32_t min_t = (int32_t)luaL_checkinteger(L, 2);

    reset();

    min_time_ = min_t;
    hooking_ = true;
    tl_active_ = this;

    // 给当前状态添加hook
    lua_sethook(L, hook_cb, LUA_MASKCALL | LUA_MASKRET, 0);

    return 0;
}

// Lua: profiler:end_hook(path)
int Profile::end_hook(lua_State *L)
{
    const char *path = luaL_checkstring(L, 2);

    // 解除当前状态的hook
    lua_sethook(L, nullptr, 0, 0);
    hooking_ = false;

    // 挂起的线程，全部停止计时，修正挂起时间。没回调的一律按当前时间算
    for (auto& x : stacks_)
    {
        remove_suspend_time(x.second);
    }

    write_report(path);
    reset();

    tl_active_ = nullptr;

    return 0;
}

void Profile::remove_suspend_time(CoState &cs)
{
    if (0 == cs.suspend_time) return;

    int64_t now = timing::steady_clock();
    int64_t dt  = now - cs.suspend_time;
    for (auto &frame : cs.stack)
    {
        frame.enter_time += dt;
    }
    cs.suspend_time = 0;
}

// Lua: profiler:set_hook(co, enable)
int Profile::set_hook(lua_State *L)
{
    lua_State *co = lua_tothread(L, 2);
    bool enable = lua_toboolean(L, 3);

    if (!hooking_ || !co) return 0;

    if (enable)
    {
        // 给协程添加hook
        lua_sethook(co, hook_cb,
                    LUA_MASKCALL | LUA_MASKRET, 0);

        // 补偿挂起时间：将栈中所有帧的enter_time往后推移
        auto it = stacks_.find(co);
        if (it != stacks_.end())
        {
            remove_suspend_time(it->second);
        }
    }
    else
    {
        // 解除协程的hook
        lua_sethook(co, nullptr, 0, 0);

        auto it = stacks_.find(co);
        if (it != stacks_.end())
        {
            if (it->second.stack.empty())
            {
                // 堆栈为空，删除条目避免泄漏
                stacks_.erase(it);
            }
            else
            {
                // 记录挂起时间点
                it->second.suspend_time =
                    timing::steady_clock();
            }
        }
    }

    return 0;
}

// ==================== 报告输出 ====================

void Profile::write_report(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp) return;

    fprintf(fp, "%-10s %-10s %-10s %-10s %-10s %s\n",
            "time", "times", "avg", "min", "max",
            "stack");
    fprintf(fp,
            "----------------------------------------"
            "----------------------------------------\n");

    // 顶层按time从大到小排序
    std::sort(root_.children_vec.begin(),
              root_.children_vec.end(),
              [](const Node *a, const Node *b)
              {
                  return a->total_time > b->total_time;
              });

    for (auto *child : root_.children_vec)
    {
        write_node(fp, child, 0);
    }

    fclose(fp);
}

void Profile::write_node(
    FILE *fp, Node *node, int depth)
{
    // 过滤低于阈值的节点
    if (node->total_time < min_time_) return;

    int64_t avg = node->times > 0
                      ? node->total_time / node->times
                      : 0;
    int64_t min_v = node->min_time == INT64_MAX
                        ? 0
                        : node->min_time;

    std::string indent;
    for (int i = 0; i < depth; ++i)
    {
        if (i == depth - 1)
            indent += "|+ ";
        else
            indent += "|  ";
    }

    fprintf(fp,
            "%-10lld %-10lld %-10lld %-10lld %-10lld "
            "%s%s\n",
            (long long)node->total_time,
            (long long)node->times,
            (long long)avg,
            (long long)min_v,
            (long long)node->max_time,
            indent.c_str(),
            node->key.c_str());

    for (auto *child : node->children_vec)
    {
        write_node(fp, child, depth + 1);
    }
}

void Profile::reset()
{
    stacks_.clear();
    for (auto *child : root_.children_vec)
    {
        delete child;
    }
    root_.children_vec.clear();
    root_.children_map.clear();
    root_.total_time = 0;
    root_.times = 0;
    root_.min_time = INT64_MAX;
    root_.max_time = 0;
}
