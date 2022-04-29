-- application.lua
-- 2018-04-04
-- xzc

-- 通用app(Application)逻辑
App = {}

-- 通用app逻辑，如网关、世界、场景...
-- 不通用的app（如单元测试，机器人）不需要走这里的逻辑

require "global.name"

-- 定义一些常用的变量，方便程序调用时直接使用而不需要到处去计算
GSE = GSE -- 网关session id
WSE = WSE -- 世界服session id
ASE = ASE -- 场景1服session id
APP_TYPE = APP_TYPE -- 当前app的类型id

local g_app = g_app
local next_gc = 0 -- 下一次执行luagc的时间，不影响热更
local gc_counter = 0 -- 完成gc的次数
local gc_counter_tm = 0 -- 上一次完成gc的时间

local start_step = 0 -- 已执行的启动步骤
local start_func = {} -- 初始化步骤
local stop_step = 0 -- 已执行的关闭步骤
local stop_func = {} -- 初始化步骤

local app_ev = {} -- 主循环回调

-- 生成服务器session id
-- @param name  服务器类型，如gateway、world...，见APP类型定义
-- @param index 服务器索引，如多个gateway，分别为1,2...
-- @param id 服务器id，与运维相关。开了第N个服
function App.encode_session(app_type, index, id)
    assert(app_type, "server name type not define")
    assert(index < (1 << 24), "server index out of boundry")
    assert(id < (1 << 16), "server id out of boundry")

    -- int32 ,8bits is ty,8bits is index,16bits is id
    return (app_type << 24) + (index << 16) + id
end

-- 解析session id
-- @return 服务器id，服务器索引，服务器id
function App.decode_session(session)
    local ty = session >> 24;
    local index = (session >> 16) & 0xFF
    local id = session & 0xFFFF

    return ty, index, id
end

-- 注册主事件回调，一般仅仅在场景和实体中使用
function App.reg_ev(func)
    for _, old_func in ipairs(app_ev) do
        assert(func ~= old_func)
    end

    table.insert(app_ev, func)
end

-- 注册app启动回调
-- @param name 模块名
-- @param func 回调函数，该返回必须返回初始化信息
-- @param pr 优先级priority，越小优先级越高，默认20
function App.reg_start(name, func, pr)
    table.insert(start_func, {
        pr = pr or 20,
        name = name,
        func = func
    })
end

-- 注册app关闭回调
-- @param name 模块名
-- @param func 回调函数，该返回必须返回关闭信息
-- @param pr 优先级priority，越小优先级越高，默认20
function App.reg_stop(name, func, pr)
    table.insert(stop_func, {
        pr = pr or 20,
        name = name,
        func = func
    })
end

-- 初始化完成
local function end_start()
    if g_app.start_timer then
        Timer.stop(g_app.start_timer)
        g_app.start_timer = nil
    end

    g_app.ok = true
    if g_app.ev_interval then
        ev:set_app_ev(g_app.ev_interval) -- N毫秒回调一次主循环
    end

    printf("App %s(index = %d, id = %d, 0x%.8X) start OK", g_app.name,
        g_app.index, g_app.id, g_app.session)

    SE.fire_event(SE_READY, g_app.name, g_app.index, g_app.id, g_app.session)
end

-- 执行下一个app初始化步骤
local function do_start()
    if start_step >= #start_func then
        return end_start()
    end

    start_step = start_step + 1
    local step = start_func[start_step]

    printf("starting %d/%d: %s",
        start_step, #start_func,  step.name)

    step.tm = ev:time()
    local ok = step.func()

    -- 不需要异步的初始化，直接执行下一步。异步的则由定时器处理
    if ok then
        printf("starting %d/%d: %s OK",
            start_step, #start_func, step.name)

        do_start()
    end
end

-- 进程初始化
local function initialize()
    local opts = g_app.opts

    g_app.name = opts.app
    g_app.index = tonumber(opts.index) or 1
    g_app.id = tonumber(opts.id) or 1

    g_app.start_time = ev:time()

    local id = g_app.id
    GSE = App.encode_session(GATEWAY, 1, id)
    WSE = App.encode_session(WORLD, 1, id)
    ASE = App.encode_session(AREA, 1, id)

    APP_TYPE = APP[string.upper(g_app.name)]
    g_app.session = App.encode_session(APP_TYPE, g_app.index, g_app.id)

    -- 设置当前session到C++
    network_mgr:set_curr_session(g_app.session)
end

-- 检测哪些初始化未完成
local function check_start()
    local now = ev:time()
    local step = start_func[start_step]

    if step.func(true) then
        printf("starting %d/%d: %s OK",
            start_step, #start_func,  step.name)

        do_start()
        return
    end

    if now - step.tm > 10 then
        step.tm = ev:time()
        printf("starting %d/%d: %s ...",
            start_step, #start_func, step.name)
    end
end

-- 开始执行模块启动步骤
local function beg_start()
    if 0 == #start_func then return end_start() end

    -- 通过定时器检测初始化是否完成
    g_app.start_timer = Timer.interval(200, 200, -1, check_start)

    -- pr值越小，优先级越高
    table.sort(start_func, function(a, b) return a.pr < b.pr end)
    do_start()
end

-- 最终关服需要执行的逻辑处理
local function end_stop()
    ev:exit()
end

-- 执行关服步骤
local function do_stop()
    if stop_step >= #stop_func then
        return end_stop()
    end

    stop_step = stop_step + 1
    local step = stop_func[stop_step]

    printf("stopping %d/%d: %s",
        stop_step, #stop_func,  step.name)

    step.tm = ev:time()
    local ok = step.func()

    -- 不需要异步的初始化，直接执行下一步。异步的则由定时器处理
    if ok then
        printf("stopping %d/%d: %s OK",
            stop_step, #stop_func, step.name)

        do_stop()
    end
end

-- 检测关服是否完成
local function check_stop()
    local now = ev:time()
    local step = stop_func[stop_step]

    if step.func(true) then
        printf("stopping %d/%d: %s OK",
            stop_step, #stop_func, step.name)

        do_stop()
        return
    end

    if now - step.tm > 10 then
        step.tm = ev:time()
        printf("stopping %d/%d: %s ...",
            stop_step, #stop_func, step.name)
    end
end

-- 开始执行关服
local function beg_stop()
    -- 如果起服时失败，直接关服，走关服步骤毫无意义
    if not g_app.ok then
        print("app NEVER Successfully started, shutdown immediately !")
        end_stop()
        return
    end

    if 0 == #stop_func then return end_stop() end

    -- 通过定时器检测关服是否完成
    g_app.stop_timer = Timer.interval(200, 200, -1, check_stop)

    -- pr值越小，优先级越高
    table.sort(stop_func, function(a, b) return a.pr < b.pr end)
    do_stop()
end

-- 加载各个功能模块
function App.load_module()
    g_app.ready = false
    require(g_app.module_boot_file)

    -- 生成函数及其对应的名字
    make_name()

    -- 一些需要依赖其他脚本初始化的，请使用这个事件来初始化
    -- 这样不需要考虑脚本加载的先后顺序，这个事件触发时所有脚本都加载完成了
    __fire_sys_ev(SE_SCRIPT_LOADED)
    g_app.ready = true
end

-- 运行进程
function App.exec()
    -- 停用自动增量gc，在主循环里手动调用(TODO: 测试5.4的新gc效果)
    collectgarbage("stop")

    -- 注册关服信号
    ev:signal(2)
    ev:signal(15)

    require_define "modules.system.define"

    -- 执行初始化
    initialize()

    App.load_module()

    -- 执行各个模块的启动步骤
    beg_start()

    ev:backend()
end

-- /////////////////////////////////////////////////////////////////////////////
-- 下面这几个是由C++底层调用
-- /////////////////////////////////////////////////////////////////////////////

-- 信号处理，默认情况下退出
--[[
/usr/include/bits/signum.h

/* Fake signal functions.  */
#define SIG_ERR	((__sighandler_t) -1)		/* Error return.  */
#define SIG_DFL	((__sighandler_t) 0)		/* Default action.  */
#define SIG_IGN	((__sighandler_t) 1)		/* Ignore signal.  */

#ifdef __USE_UNIX98
# define SIG_HOLD	((__sighandler_t) 2)	/* Add signal to hold mask.  */
#endif


/* Signals.  */
#define	SIGHUP		1	/* Hangup (POSIX).  */
#define	SIGINT		2	/* Interrupt (ANSI).  */
#define	SIGQUIT		3	/* Quit (POSIX).  */
#define	SIGILL		4	/* Illegal instruction (ANSI).  */
#define	SIGTRAP		5	/* Trace trap (POSIX).  */
#define	SIGABRT		6	/* Abort (ANSI).  */
#define	SIGIOT		6	/* IOT trap (4.2 BSD).  */
#define	SIGBUS		7	/* BUS error (4.2 BSD).  */
#define	SIGFPE		8	/* Floating-point exception (ANSI).  */
#define	SIGKILL		9	/* Kill, unblockable (POSIX).  */
#define	SIGUSR1		10	/* User-defined signal 1 (POSIX).  */
#define	SIGSEGV		11	/* Segmentation violation (ANSI).  */
#define	SIGUSR2		12	/* User-defined signal 2 (POSIX).  */
#define	SIGPIPE		13	/* Broken pipe (POSIX).  */
#define	SIGALRM		14	/* Alarm clock (POSIX).  */
#define	SIGTERM		15	/* Termination (ANSI).  */
#define	SIGSTKFLT	16	/* Stack fault.  */
#define	SIGCLD		SIGCHLD	/* Same as SIGCHLD (System V).  */
#define	SIGCHLD		17	/* Child status has changed (POSIX).  */
#define	SIGCONT		18	/* Continue (POSIX).  */
#define	SIGSTOP		19	/* Stop, unblockable (POSIX).  */
#define	SIGTSTP		20	/* Keyboard stop (POSIX).  */
#define	SIGTTIN		21	/* Background read from tty (POSIX).  */
#define	SIGTTOU		22	/* Background write to tty (POSIX).  */
#define	SIGURG		23	/* Urgent condition on socket (4.2 BSD).  */
#define	SIGXCPU		24	/* CPU limit exceeded (4.2 BSD).  */
#define	SIGXFSZ		25	/* File size limit exceeded (4.2 BSD).  */
#define	SIGVTALRM	26	/* Virtual alarm clock (4.2 BSD).  */
#define	SIGPROF		27	/* Profiling alarm clock (4.2 BSD).  */
#define	SIGWINCH	28	/* Window size change (4.3 BSD, Sun).  */
#define	SIGPOLL		SIGIO	/* Pollable event occurred (System V).  */
#define	SIGIO		29	/* I/O now possible (4.2 BSD).  */
#define	SIGPWR		30	/* Power failure restart (System V).  */
#define SIGSYS		31	/* Bad system call.  */
#define SIGUNUSED	31
]]
local sig_action = {} -- 注意，这个热更要重新注册。关服的话为默认action则无所谓

function sig_handler(signum)
    if sig_action[signum] then return sig_action[signum]() end

    printf("catch signal %d, shutting down ...", signum)

    beg_stop()
end

-- 主循环，由C++回调
function application_ev(ms_now)
    for _, func in ipairs(app_ev) do
        func(ms_now)
    end

    if ms_now > next_gc then
        next_gc = ms_now + 1000 -- 多久执行一次？
        if collectgarbage("step", 100) then
            -- 当脚本占用的内存比较低时，gc完成的时间很快的，需要控制一下日志打印的速度
            gc_counter = gc_counter + 1
            if ms_now - gc_counter_tm > 60000 then
                printf("gc finished %d times, now use mem %f kb", gc_counter,
                       collectgarbage("count"))
                gc_counter = 0
                gc_counter_tm = ms_now
            end
        end
    end
end
-- /////////////////////////////////////////////////////////////////////////////

name_func("App.check_stop", check_stop)
name_func("App.check_start", check_start)

return App
