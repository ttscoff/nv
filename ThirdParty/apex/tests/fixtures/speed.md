### Network speed history chart (`waveform`)

Run Ookla speedtest CLI and convert results to waveform widgets. Requires [the Ookla version](https://www.speedtest.net/apps/cli), not the Homebrew version.

Add medium widgets with ids "download" and "upload" to use this, and edit the bin variables at the top to match your setup. This script can be run in the background using launchd.

Append each speed test result to a history file, then render the latest 20 samples:

```bash
#!/usr/bin/env bash
# Example: Speedtest background widget updater
# runs speedtest, logs, and updates terminal-widget waveform widgets.
# Requires: /usr/local/bin/speedtest, /opt/homebrew/bin/terminal-widget
# Add medium widgets with ids "download" and "upload" to your Desktop/iPhone.
set -e

# Set paths to executables here
SPEEDTEST_BIN="/usr/local/bin/speedtest"
TERMINAL_WIDGET_BIN="/opt/homebrew/bin/terminal-widget"

LOGDIR="$HOME/logs"
LOGFILE="$LOGDIR/speed.csv"
mkdir -p "$LOGDIR"

WIDGET_COUNT=20

# Helper to get values from log
gather_log_values() {
	local count="$1"
	local col="$2"
	tail -n "$count" "$LOGFILE" | awk -F, -v c="$col" '{print ($c==""?0:$c)}'
}

# Run speedtest and log result
run_speedtest() {
	local jsonfile
	jsonfile=$(mktemp)
	"$SPEEDTEST_BIN" -f json -p no >"$jsonfile"
	local status=$?
	if [[ $status -ne 0 ]]; then
		rm -f "$jsonfile"
		exit $status
	fi
	read -r DOWNLOAD UPLOAD < <(python3 -c '
import json,sys
with open(sys.argv[1]) as f:
  data=json.load(f)
  def calc(b): return int(round(b/(1024*100)))
  print(calc(data["download"]["bandwidth"]), calc(data["upload"]["bandwidth"]))
' "$jsonfile")
	rm -f "$jsonfile"
	TIMESTAMP=$(date -Iseconds)
	echo "$TIMESTAMP,$DOWNLOAD,$UPLOAD" >>"$LOGFILE"
	tmpfile=$(mktemp)
	tail -n 100 "$LOGFILE" >"$tmpfile"
	mv "$tmpfile" "$LOGFILE"
}

# Update terminal-widget waveform widgets for both download and upload
update_widgets() {
	DOWN_VALS=($(gather_log_values "$WIDGET_COUNT" 2))
	UP_VALS=($(gather_log_values "$WIDGET_COUNT" 3))
	printf "%s " "${DOWN_VALS[@]}" | sed 's/ $//' | "$TERMINAL_WIDGET_BIN" --target download --text "Download Speeds" --caption --chart - --chart-format waveform --bg ffffff --fg "rgb(237, 78, 195)" --text-color 222222 --caption-color 888888
	printf "%s " "${UP_VALS[@]}" | sed 's/ $//' | "$TERMINAL_WIDGET_BIN" --target upload --text "Upload Speeds" --caption --chart - --chart-format waveform --bg ffffff --fg "rgb(78, 155, 237)" --text-color 222222 --caption-color 888888
}

# Main: always run speedtest and update widgets
run_speedtest
update_widgets
```

Here's a luanchd example for the speedtest script. This assumes you've saved the script as speed-widget.sh, and you'll need to update your path in the ProgramArguments.

```
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>Label</key>
	<string>com.brettterpstra.speed</string>
	<key>ProgramArguments</key>
	<array>
		<string>/Users/ttscoff/scripts/speed-widget.sh</string>
	</array>
	<key>RunAtLoad</key>
	<true/>
	<key>StandardErrorPath</key>
	<string>/Users/ttscoff/logs/com.brettterpstra.speedtest.err</string>
	<key>StandardOutPath</key>
	<string>/Users/ttscoff/logs/com.brettterpstra.speedtest.out</string>
	<key>StartInterval</key>
	<integer>900</integer>
</dict>
</plist>

```
### Network speed history chart (`waveform`)

Run Ookla speedtest CLI and convert results to waveform widgets. Requires [the Ookla version](https://www.speedtest.net/apps/cli), not the Homebrew version.

Add medium widgets with ids "download" and "upload" to use this, and edit the bin variables at the top to match your setup. This script can be run in the background using launchd.

Append each speed test result to a history file, then render the latest 20 samples:

```bash
#!/usr/bin/env bash
# Example: Speedtest background widget updater
# runs speedtest, logs, and updates terminal-widget waveform widgets.
# Requires: /usr/local/bin/speedtest, /opt/homebrew/bin/terminal-widget
# Add medium widgets with ids "download" and "upload" to your Desktop/iPhone.
set -e

# Set paths to executables here
SPEEDTEST_BIN="/usr/local/bin/speedtest"
TERMINAL_WIDGET_BIN="/opt/homebrew/bin/terminal-widget"

LOGDIR="$HOME/logs"
LOGFILE="$LOGDIR/speed.csv"
mkdir -p "$LOGDIR"

WIDGET_COUNT=20

# Helper to get values from log
gather_log_values() {
	local count="$1"
	local col="$2"
	tail -n "$count" "$LOGFILE" | awk -F, -v c="$col" '{print ($c==""?0:$c)}'
}

# Run speedtest and log result
run_speedtest() {
	local jsonfile
	jsonfile=$(mktemp)
	"$SPEEDTEST_BIN" -f json -p no >"$jsonfile"
	local status=$?
	if [[ $status -ne 0 ]]; then
		rm -f "$jsonfile"
		exit $status
	fi
	read -r DOWNLOAD UPLOAD < <(python3 -c '
import json,sys
with open(sys.argv[1]) as f:
  data=json.load(f)
  def calc(b): return int(round(b/(1024*100)))
  print(calc(data["download"]["bandwidth"]), calc(data["upload"]["bandwidth"]))
' "$jsonfile")
	rm -f "$jsonfile"
	TIMESTAMP=$(date -Iseconds)
	echo "$TIMESTAMP,$DOWNLOAD,$UPLOAD" >>"$LOGFILE"
	tmpfile=$(mktemp)
	tail -n 100 "$LOGFILE" >"$tmpfile"
	mv "$tmpfile" "$LOGFILE"
}

# Update terminal-widget waveform widgets for both download and upload
update_widgets() {
	DOWN_VALS=($(gather_log_values "$WIDGET_COUNT" 2))
	UP_VALS=($(gather_log_values "$WIDGET_COUNT" 3))
	printf "%s " "${DOWN_VALS[@]}" | sed 's/ $//' | "$TERMINAL_WIDGET_BIN" --target download --text "Download Speeds" --caption --chart - --chart-format waveform --bg ffffff --fg "rgb(237, 78, 195)" --text-color 222222 --caption-color 888888
	printf "%s " "${UP_VALS[@]}" | sed 's/ $//' | "$TERMINAL_WIDGET_BIN" --target upload --text "Upload Speeds" --caption --chart - --chart-format waveform --bg ffffff --fg "rgb(78, 155, 237)" --text-color 222222 --caption-color 888888
}

# Main: always run speedtest and update widgets
run_speedtest
update_widgets
```

Here's a luanchd example for the speedtest script. This assumes you've saved the script as speed-widget.sh, and you'll need to update your path in the ProgramArguments.

```
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>Label</key>
	<string>com.brettterpstra.speed</string>
	<key>ProgramArguments</key>
	<array>
		<string>/Users/ttscoff/scripts/speed-widget.sh</string>
	</array>
	<key>RunAtLoad</key>
	<true/>
	<key>StandardErrorPath</key>
	<string>/Users/ttscoff/logs/com.brettterpstra.speedtest.err</string>
	<key>StandardOutPath</key>
	<string>/Users/ttscoff/logs/com.brettterpstra.speedtest.out</string>
	<key>StartInterval</key>
	<integer>900</integer>
</dict>
</plist>

```
