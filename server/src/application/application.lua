-- application.lua
-- 2018-04-04
-- xzc
-- app进程基类
local Application = oo.class(...)

local g_app = nil
local next_gc = 0 -- 下一次执行luagc的时间，不影响热更
local gc_counter = 0 -- 完成gc的次数
local gc_counter_tm = 0 -- 上一次完成gc的时间

local start_step = 0 -- 已执行的启动步骤
local start_func = {} -- 初始化步骤
local stop_step = 0 -- 已执行的关闭步骤
local stop_func = {} -- 初始化步骤

local app_ev = {} -- 主循环回调

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

    g_app:beg_stop()
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

-- 初始化
function Application:__init()
    g_app = self

    -- 停用自动增量gc，在主循环里手动调用(TODO: 测试5.4的新gc效果)
    collectgarbage("stop")

    -- 关服信号
    ev:signal(2)
    ev:signal(15)
end

-- 注册主事件回调，一般仅仅在场景和实体中使用
function Application:reg_ev(func)
    for _, old_func in ipairs(app_ev) do
        assert(func ~= old_func)
    end

    table.insert(app_ev, func)
end

-- 注册app启动回调
-- @param name 模块名
-- @param func 回调函数，该返回必须返回初始化信息
-- @param pr 优先级priority，越小优先级越高，默认20
function Application:reg_start(name, func, pr)
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
function Application:reg_stop(name, func, pr)
    table.insert(stop_func, {
        pr = pr or 20,
        name = name,
        func = func
    })
end

-- 执行下一个app初始化步骤
function Application:do_start()
    if start_step >= #start_func then
        return self:end_start()
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

        self:do_start()
    end
end

-- 进程初始化
function Application:initialize()
    if 0 == #start_func then return self:end_start() end

    -- 通过定时器检测初始化是否完成
    self.start_timer = g_timer_mgr:interval(200, 200, -1, self,
                                                 self.check_start)

    -- pr值越小，优先级越高
    table.sort(start_func, function(a, b) return a.pr < b.pr end)
    self:do_start()
end

-- 检测哪些初始化未完成
function Application:check_start()
    local now = ev:time()
    local step = start_func[start_step]

    if step.func(true) then
        printf("starting %d/%d: %s OK",
            start_step, #start_func,  step.name)

        self:do_start()
        return
    end

    if now - step.tm > 10 then
        step.tm = ev:time()
        printf("starting %d/%d: %s ...",
            start_step, #start_func, step.name)
    end
end

-- 初始化完成
function Application:end_start()
    if self.start_timer then  g_timer_mgr:stop(self.start_timer) end

    self.ok = true
    SE.fire_event(SE_READY, self.name, self.index, self.id, self.session)
end

-- 检测关服是否完成
function Application:check_stop()
    local now = ev:time()
    local step = stop_func[stop_step]

    if step.func(true) then
        printf("stopping %d/%d: %s OK",
            stop_step, #stop_func, step.name)

        self:do_stop()
        return
    end

    if now - step.tm > 10 then
        step.tm = ev:time()
        printf("stopping %d/%d: %s ...",
            stop_step, #stop_func, step.name)
    end
end

-- 执行关服步骤
function Application:do_stop()
    if stop_step >= #stop_func then
        return self:end_stop()
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

        self:do_stop()
    end
end

-- 开始执行关服
function Application:beg_stop()
    -- 如果起服时失败，直接关服，走关服步骤毫无意义
    if not self.ok then
        print("app NEVER Successfully started, shutdown immediately !")
        self:end_stop()
        return
    end

    if 0 == #stop_func then return self:end_stop() end

    -- 通过定时器检测关服是否完成
    self.check_stop_timer = g_timer_mgr:interval(200, 200, -1, self,
                                                 self.check_stop)

    -- pr值越小，优先级越高
    table.sort(stop_func, function(a, b) return a.pr < b.pr end)
    self:do_stop()
end

-- 关服处理
function Application:end_stop()
    ev:exit()
end

-- 运行进程
function Application:exec()
    self:initialize()

    ev:backend()
end

return Application
