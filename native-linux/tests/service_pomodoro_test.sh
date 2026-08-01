#!/bin/sh
set -eu

service=$1
cli=$2
test_dir=$(mktemp -d /tmp/crossdashboard-service-test.XXXXXX)
export XDG_DATA_HOME="$test_dir/data"
export XDG_CONFIG_HOME="$test_dir/config"
mkdir -p "$XDG_DATA_HOME" "$XDG_CONFIG_HOME"

cleanup() {
  kill -KILL "$service_pid" 2>/dev/null || true
  wait "$service_pid" 2>/dev/null || true
  [ ! -d "$test_dir" ] || find "$test_dir" -depth -delete
}
trap cleanup EXIT

"$service" >/dev/null 2>&1 &
service_pid=$!

attempt=0
until gdbus call --session --dest com.crossdashboard.Service \
  --object-path /com/crossdashboard/Service \
  --method com.crossdashboard.Service1.GetPomodoroState >/dev/null 2>&1; do
  attempt=$((attempt + 1))
  [ "$attempt" -lt 50 ] || exit 1
  sleep 0.1
done

first=$(gdbus call --session --dest com.crossdashboard.Service \
  --object-path /com/crossdashboard/Service \
  --method com.crossdashboard.Service1.StartPomodoro task test-uid "Test focus" focus 1)
[ "$first" = "(true,)" ]

second=$(gdbus call --session --dest com.crossdashboard.Service \
  --object-path /com/crossdashboard/Service \
  --method com.crossdashboard.Service1.StartPomodoro task second-uid "Second focus" focus 1)
[ "$second" = "(false,)" ]

"$cli" pomo status | grep -F "Test focus" >/dev/null
waybar_line=$(timeout 2 "$cli" waybar | head -n 1)
printf '%s\n' "$waybar_line" | grep -F '"class":"pomodoro"' >/dev/null
"$cli" pomo pause | grep -F "paused" >/dev/null
"$cli" pomo status | grep -F "paused" >/dev/null
"$cli" pomo resume | grep -F "resumed" >/dev/null
"$cli" pomo stop | grep -F "stopped" >/dev/null
"$cli" pomo status | grep -F "No active Pomodoro" >/dev/null
