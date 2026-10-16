#!/usr/bin/env bash
# 通用运行器：每个模块一个 worker，全部限制在工作目录内。
# 用法: scripts/loop_universal.sh start|stop|status
# 注意：shell 变量名只能用 ASCII（POSIX 限制），注释用中文。
set -u
WORK_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STATE_DIR="$WORK_DIR/.agents"
LOG_FILE="$STATE_DIR/loop.log"
mkdir -p "$STATE_DIR"
MODULES="容器 显示 输入 存储 网络 无根 剪贴板 界面 诊断 工具"

do_start() {
  for m in $MODULES; do
    if pgrep -f "hourly_loop.sh $m" >/dev/null 2>&1; then
      echo "worker $m 已在运行"
    else
      nohup "$WORK_DIR/scripts/hourly_loop.sh" "$m" >>"$LOG_FILE" 2>&1 &
      echo "worker $m 已启动 pid=$!"
    fi
  done
  echo "全部 10 个模块 worker 已启动，日志: $LOG_FILE"
}

do_stop() { pkill -f "hourly_loop.sh" 2>/dev/null; echo "已停止所有 worker"; }
do_status() { pgrep -af "hourly_loop.sh" || echo "无运行中的 worker"; }

case "${1:-status}" in
  start) do_start;; stop) do_stop;; status) do_status;; *) echo "用法: $0 start|stop|status"; exit 1;;
esac
