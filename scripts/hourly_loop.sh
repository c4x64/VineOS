#!/usr/bin/env bash
# VineOS 每小时 10万有效行循环 —— 严格限制在工作目录内。
# 用法: scripts/hourly_loop.sh [模块名]（默认：全部）
# 状态/日志：工作目录下的 .agents/。绝不写到目录外。
# 注意：shell 变量名只能用 ASCII（POSIX 限制），注释与提交信息用中文。
set -u
WORK_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STATE_DIR="$WORK_DIR/.agents"
LOG_FILE="$STATE_DIR/loop.log"
TMP_DIR="$STATE_DIR/tmp"
mkdir -p "$STATE_DIR" "$TMP_DIR"

TARGET_LINES=100000
MODULE="${1:-全部}"

log_msg() { echo "[$(date '+%Y-%m-%d %H:%M:%S')] [$MODULE] $*" | tee -a "$LOG_FILE"; }

# 保守限制：拒绝任何解析到工作目录之外的路径
inside_workdir() {
  local p="$1" real
  real="$(cd "$WORK_DIR" && realpath -m "$p" 2>/dev/null || echo "$WORK_DIR/$p")"
  case "$real" in "$WORK_DIR"/*) return 0;; *) return 1;; esac
}

log_msg "启动，每小时目标 ${TARGET_LINES} 有效行，模块=${MODULE}"
while true; do
  STAMP="$(date '+%Y%m%d_%H%M%S')"
  BATCH_DIR="$STATE_DIR/批次_${MODULE}_${STAMP}"
  mkdir -p "$BATCH_DIR"
  log_msg "本轮批次目录: $BATCH_DIR — 等待工作进程写入后校验（见 loop_universal.sh）"
  TOTAL=$(find "$WORK_DIR/app" -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.kt' \) -exec cat {} + 2>/dev/null | grep -vE '^[[:space:]]*(//|#|\*|$)' | grep -vcE '^[[:space:]]*$')
  log_msg "当前工作区有效代码行(近似): $TOTAL / 1000000"
  echo "$STAMP $TOTAL" >> "$STATE_DIR/进度.txt"
  log_msg "休眠 3600 秒"
  sleep 3600
done
