#!/usr/bin/env ruby
require 'English'

errors_only = "true" =~ /^[ty]/i
suite = "all"
out = ""
code = 0

def normalize_suite(suite)
  files = Dir.glob("tests/test_*.rb")
  suite = suite.split('').join('.*')
  if suite && !suite.empty?
    files.delete_if { |f| f !~ /#{suite}/  }
  end
  files.map { |f| File.basename(f, '.rb')  }
end

if suite =~ /^all/i
  out = `./build/apex_test_runner 2>&1`
  code = $CHILD_STATUS.exitstatus
else

  test_files = normalize_suite(suite)

  test_files.each do |file|
    out += `./build/apex_test_runner #{file} 2>&1`
    if $CHILD_STATUS.exitstatus != 0
      code = $CHILD_STATUS.exitstatus
    end
  end
end

if errors_only
  puts out.lines.select { |line| line =~ /✗/ }.join
else
  puts out
end

exit code