#!/bin/bash
# Debug script for apex_test_runner

echo "=== Running apex_test_runner in lldb ==="
echo ""
echo "Commands will be:"
echo "  (lldb) run"
echo "  (lldb) bt          # when it crashes, this shows the stack trace"
echo "  (lldb) frame select 0  # select the top frame"
echo "  (lldb) print *write     # print variables"
echo ""

cd "$(dirname "$0")"
lldb build/apex_test_runner <<EOF
run
bt
frame select 0
print remaining
print write
print output
quit
EOF
