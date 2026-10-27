#!/bin/bash
# ============================================================
# 进入 VirtualBox 虚拟机(Debian 12)执行一条命令，用 Windows 自带的 ssh + 密码登录
#
# 用法:
#   bash vm_sh.sh "sh shell/build.sh"          # 在工作目录里执行
#   bash vm_sh.sh "uname -a; ls /media/sf_dev" # 多条命令用 ; 或 && 串起来
#   bash vm_sh.sh -P 2222 -l root -w 123456 "ls"  # 登录信息变了用参数覆盖
#   bash vm_sh.sh --interactive                 # 交互式登录(自己输密码)
#
# 默认: 用户 xzc  密码 1  地址 127.0.0.1:1883  工作目录 /media/sf_dev/MServer
# 登录信息(端口/用户/密码/地址)可能会变，用户给了新的就优先用用户给的
#
# 参数: -H <地址> -P <端口> -l <用户> -w <密码> -d <工作目录> --no-cd --interactive
#
# 说明:
#   - ssh 不能在命令行上带密码，这里借 SSH_ASKPASS 传。生效有两个前提，缺一个都会退化成
#     读终端然后报 Permission denied: stdin 不是 tty(脚本里重定向 < /dev/null)、DISPLAY 非空
#   - 全程不碰 ~/.ssh: 关掉公钥认证(IdentitiesOnly + PubkeyAuthentication=no)，
#     也不写 known_hosts(UserKnownHostsFile=/dev/null + StrictHostKeyChecking=no)
#   - 密码通过环境变量传给 askpass 脚本，避免引号/特殊字符把脚本截断
# ============================================================

set -u

VM_HOST="${VM_HOST:-127.0.0.1}"
VM_PORT="${VM_PORT:-1883}"
VM_USER="${VM_USER:-xzc}"
VM_PWD="${VM_PWD:-1}"
VM_DIR="${VM_DIR:-/media/sf_dev/MServer}"

CD_DIR=1
INTERACTIVE=0
CMD=""
ASKPASS=""

usage() {
    sed -n '2,26p' "$0" | sed -e 's/^# \{0,1\}//'
    exit 0
}

cleanup() {
    [ -n "$ASKPASS" ] && rm -f "$ASKPASS"
}
trap cleanup EXIT

while [ $# -gt 0 ]; do
    case "$1" in
        -H) VM_HOST=$2; shift 2 ;;
        -P) VM_PORT=$2; shift 2 ;;
        -l) VM_USER=$2; shift 2 ;;
        -w) VM_PWD=$2;  shift 2 ;;
        -d) VM_DIR=$2;  shift 2 ;;
        --no-cd) CD_DIR=0; shift ;;
        --interactive) INTERACTIVE=1; shift ;;
        -h|--help) usage ;;
        -*) echo "[vm] 未知参数: $1 (用 -h 看用法)"; exit 1 ;;
        *) CMD=$1; shift ;;
    esac
done

if [ "$VM_PORT" -le 0 ] 2>/dev/null; then
    echo "[vm] 端口不合法: $VM_PORT"
    exit 1
fi

SSH_OPTS=(
    -p "$VM_PORT"
    -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR
    -o PubkeyAuthentication=no -o IdentitiesOnly=yes -o PreferredAuthentications=password
    -o ConnectTimeout=8
)

if [ "$INTERACTIVE" -eq 1 ]; then
    echo "[vm] 交互登录 $VM_USER@$VM_HOST:$VM_PORT (密码自己输)"
    exec ssh -t "${SSH_OPTS[@]}" "$VM_USER@$VM_HOST"
fi

if [ -z "$CMD" ]; then
    echo "[vm] 缺少要执行的命令，交互登录请加 --interactive"
    exit 1
fi

# ssh 要一个"程序"作为 askpass，只能临时落一个文件，退出时删掉
ASKPASS=$(TMPDIR=/tmp mktemp) || exit 1
printf '#!/bin/sh\nprintf "%%s" "$VM_PASSWORD"\n' > "$ASKPASS" || exit 1
chmod +x "$ASKPASS" || exit 1

if [ "$CD_DIR" -eq 1 ] && [ -n "$VM_DIR" ]; then
    CMD="cd $VM_DIR && $CMD"
fi

echo "[vm] $VM_USER@$VM_HOST:$VM_PORT \$ $CMD"
DISPLAY=:0 SSH_ASKPASS="$ASKPASS" VM_PASSWORD="$VM_PWD" \
    ssh "${SSH_OPTS[@]}" "$VM_USER@$VM_HOST" "$CMD" </dev/null
RC=$?

if [ "$RC" -ne 0 ]; then
    echo "[vm] 命令返回 $RC"
    echo "[vm] Permission denied -> 密码/用户不对，用 -w/-l 覆盖"
    echo "[vm] Connection refused -> 端口不对(虚拟机端口转发可能变了)，用 -P 覆盖"
fi
exit "$RC"
