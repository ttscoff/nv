#!/System/Library/Frameworks/Ruby.framework/Versions/Current/usr/bin/ruby
# encoding: utf-8
# Usage: tp2md.rb filename.taskpaper > output.md
require 'fileutils'

def class_exists?(class_name)
  klass = Module.const_get(class_name)
  return klass.is_a?(Class)
rescue NameError
  return false
end

if class_exists? 'Encoding'
  Encoding.default_external = Encoding::UTF_8 if Encoding.respond_to?('default_external')
  Encoding.default_internal = Encoding::UTF_8 if Encoding.respond_to?('default_internal')
  input = STDIN.read.force_encoding('utf-8')
else
  input = STDIN.read
end

header = input.scan(/Format\: .*$/)
output = ""
prevlevel = 0
begin
    input.split("\n").each {|line|
      if line =~ /^(\t*)(.*?):(\s(.*?))?$/
        tabs = $1
        project = $2
        if tabs.nil?
          output += "\n## #{project} ##\n\n"
          prevlevel = 0
        else
          output += "#{tabs.gsub(/^\t/,"")}* **#{project.gsub(/^\s*-\s*/,'')}**\n"
          prevlevel = tabs.length
        end
      elsif line =~ /^(\t*)\- (.*)$/
        task = $2
        tabs = $1.nil? ? '' : $1
        task = "*<del>#{task}</del>*" if task =~ /@done/
        if tabs.length - prevlevel > 1
          tabs = "\t"
          prevlevel.times {|i| tabs += "\t"}
        end
        tabs = '' if prevlevel == 0 && tabs.length > 1
        output += "#{tabs.gsub(/^\t/,'')}* #{task.strip}\n"
        prevlevel = tabs.length
      else
        next if line =~ /^\s*$/
        tabs = ""
        (prevlevel).times {|i| tabs += "\t"}
        output += "\n#{tabs}*#{line.strip}*\n"
      end
    }
rescue => err
    puts "Exception: #{err}"
    err
end

puts header.join("\n") + "\n" unless header.nil?
puts "<style>.tag strong {font-weight:normal;color:#555} .tag a {text-decoration:none;border:none;color:#777}</style>"
puts output.gsub(/\[\[(.*?)\]\]/,"<a href=\"nvalt://find/\\1\">\\1</a>").gsub(/(@[^ \n\r\(]+)((\()([^\)]+)(\)))?/,"<em class=\"tag\"><a href=\"nvalt://find/\\0\">\\1\\3<strong>\\4</strong>\\5</a></em>")
