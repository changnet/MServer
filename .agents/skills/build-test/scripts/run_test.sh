#!/bin/bash
# ============================================================
# 启动单元测试节点 -> 等测试跑完 -> 结束进程 -> 输出本次运行日志
#
# 用法:
#   bash run_test.sh                              # 跑 test1 的全部测试
#   bash run_test.sh test1 --filter kcp --skip udp
#   bash run_test.sh test1 --filter kcp --timeout 120
#
# 说明:
#   - 节点名必须带数字(test1)，只写 test 会在 startup.lua 断言失败
#   - --filter 是 lua 模式匹配，测试标题里不要出现 '-' 这种量词字符
#   - --skip 可用 ; 分隔多个模式，如 --skip "udp;kcp"
#   - 测试跑完进程不会自己退出，脚本检测到 "N passing, M failing" 后结束它
# ============================================================

ROOT=$(cd "$(dirname "$0")/../../../.." && pwd)
SHELL_DIR="$ROOT/shell"
LOG_DIR="$ROOT/server/var/log"

NODE="test1"
TIMEOUT=600
ARGS=()

usage() {
    echo "用法: bash run_test.sh [节点] [--filter 模式] [--skip \"a;b\"] [--timeout 秒]"
    exit 0
}

while [ $# -gt 0 ]; do
    case "$1" in
        --timeout) TIMEOUT=$2; shift 2 ;;
        --node)    NODE=$2;    shift 2 ;;
        --filter|--skip|--setting) ARGS+=("$1" "$2"); shift 2 ;;
        -h|--help) usage ;;
        --*)       ARGS+=("$1"); shift ;;
        *)         NODE=$1; shift ;;
    esac
done

# 节点名 -> 日志文件名: test1 => test001 (startup.lua 的 log/%s%03d)
NODE_NAME=${NODE%%[0-9]*}
NODE_IDX=${NODE#"$NODE_NAME"}
if [ -z "$NODE_IDX" ]; then
    echo "[test] 节点名必须带数字，比如 test1"
    exit 1
fi
LOG_NAME=$(printf '%s%03d' "$NODE_NAME" "$NODE_IDX")
LOG_FILE="$LOG_DIR/$LOG_NAME"
ERR_FILE="${LOG_FILE}_error"

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) IS_WIN=1; BIN="$ROOT/server/bin/master.exe" ;;
    *)                    IS_WIN=0; BIN="$ROOT/server/bin/master" ;;
esac

if [ ! -f "$BIN" ]; then
    echo "[test] 找不到 $BIN"
    echo "[test] 先编译: shell/win_build.bat (win) 或 sh shell/build.sh (linux)"
    exit 1
fi

# 记录启动前的日志大小，只看本次运行新增的内容
OFFSET=0
[ -f "$LOG_FILE" ] && OFFSET=$(wc -c < "$LOG_FILE" | tr -d '[:space:]')
ERR_OFFSET=0
[ -f "$ERR_FILE" ] && ERR_OFFSET=$(wc -c < "$ERR_FILE" | tr -d '[:space:]')

echo "[test] node=$NODE  log=$LOG_FILE"

# 必须在 shell 目录下启动，bat/sh 里是 cd ../server 的相对路径
# 进程输出同时写日志文件，这里丢掉控制台输出避免重复
if [ $IS_WIN -eq 1 ]; then
    ( cd "$SHELL_DIR" && ./win_start.bat "$NODE" ${ARGS[@]+"${ARGS[@]}"} ) >/dev/null 2>&1 &
else
    ( cd "$SHELL_DIR" && ./start.sh "$NODE" ${ARGS[@]+"${ARGS[@]}"} ) >/dev/null 2>&1 &
fi
JPID=$!

# 轮询日志，等结果行出现；顺便处理进程崩溃退出
elapsed=0
died=0
while [ "$elapsed" -lt "$TIMEOUT" ]; do
    sleep 1
    elapsed=$((elapsed + 1))
    if [ -f "$LOG_FILE" ]; then
        SIZE=$(wc -c < "$LOG_FILE" | tr -d '[:space:]')
        # 跨天启动会轮转日志，文件变小说明换新文件了，从头读
        if [ "$SIZE" -lt "$OFFSET" ]; then
            OFFSET=0
        fi
        if [ "$SIZE" -gt "$OFFSET" ] && \
           tail -c +$((OFFSET + 1)) "$LOG_FILE" | \
           grep -qE '[0-9]+ passing, [0-9]+ failing'; then
            sleep 1 # 等日志刷盘
            break
        fi
    fi
    if ! kill -0 "$JPID" 2>/dev/null; then
        died=1
        break
    fi
done

stop_proc() {
    if [ $IS_WIN -eq 1 ]; then
        local pids
        pids=$(powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='master.exe'\" | Where-Object { \$_.CommandLine -match '--node\s+$NODE(\s|\$)' } | Select-Object -ExpandProperty ProcessId" 2>/dev/null | tr -d '\r')
        for p in $pids; do
            taskkill //F //T //PID "$p" >/dev/null 2>&1 && echo "[test] 已结束进程 pid=$p"
        done
    else
        kill -15 "$JPID" 2>/dev/null
    fi
}

[ "$died" -eq 1 ] && echo "[test] 进程提前退出，按崩溃/启动失败处理"
[ "$elapsed" -ge "$TIMEOUT" ] && [ "$died" -eq 0 ] && echo "[test] 超时(${TIMEOUT}s) 仍未看到结果行"

stop_proc
sleep 1

# 只取本次运行产生的日志
NEW=""
if [ -f "$LOG_FILE" ]; then
    SIZE=$(wc -c < "$LOG_FILE" | tr -d '[:space:]')
    [ "$SIZE" -lt "$OFFSET" ] && OFFSET=0
    NEW=$(tail -c +$((OFFSET + 1)) "$LOG_FILE" | sed -e 's/\x1b\[[0-9;]*m//g')
fi

SUMMARY=$(printf '%s\n' "$NEW" | grep -E '[0-9]+ passing, [0-9]+ failing' | tail -1 |
          sed -e 's/^\[L[^]]*\]//')
OK_CNT=$(printf '%s\n' "$NEW" | grep -c '\[  OK\]')
FAIL_CNT=$(printf '%s\n' "$NEW" | grep -c '\[FAIL\]')

echo "----------------------------------------"
if [ -n "$SUMMARY" ]; then
    echo "结果: $SUMMARY"
else
    echo "结果: 没有找到 'N passing, M failing'，测试可能没跑起来"
    echo "      手动跑 shell/win_start.bat $NODE 看控制台输出"
fi
echo "通过 $OK_CNT 条，失败 $FAIL_CNT 条"

if [ "$FAIL_CNT" -gt 0 ]; then
    echo "----------------------------------------"
    echo "失败用例(行号相对本次日志):"
    printf '%s\n' "$NEW" | grep -n '\[FAIL\]'
fi

if [ -z "$SUMMARY" ]; then
    echo "----------------------------------------"
    echo "本次日志最后 30 行:"
    printf '%s\n' "$NEW" | tail -n 30
fi

echo "----------------------------------------"
echo "日志文件: $LOG_FILE"
if [ -f "$ERR_FILE" ]; then
    ERR_SIZE=$(wc -c < "$ERR_FILE" | tr -d '[:space:]')
    [ "$ERR_SIZE" -lt "$ERR_OFFSET" ] && ERR_OFFSET=0
    if [ "$ERR_SIZE" -gt "$ERR_OFFSET" ]; then
        echo "本次错误日志:"
        tail -c +$((ERR_OFFSET + 1)) "$ERR_FILE"
    fi
fi

if [ -n "$SUMMARY" ] && [ "$FAIL_CNT" -eq 0 ]; then
    exit 0
fi
exit 1
