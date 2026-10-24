#!/usr/bin/env ruby
# Minimal Apex pre-parse plugin for {% kbd ... %} syntax
#
# Protocol:
#  - Reads a single JSON object from stdin:
#      { "version": 1, "plugin_id": "...", "phase": "pre_parse", "text": "..." }
#  - Writes the transformed markdown text to stdout (no JSON response).
#
# To use with Apex:
#   export APEX_PRE_PARSE_PLUGIN="/usr/bin/env ruby /path/to/apex/examples/kbd_plugin.rb"
#   apex input.md

require 'json'

payload = JSON.parse($stdin.read)
text = payload['text'] || ''

class String
  def clean_combo
    gsub!(/(?<=\S)-(?=\S)/, ' ')
    gsub!(/\b(comm(and)?|cmd|clover)\b/i, '@')
    gsub!(/\b(cont(rol)?|ctr?l)\b/i, '^')
    gsub!(/\b(opt(ion)?|alt)\b/i, '~')
    gsub!(/\bshift\b/i, '$')
    gsub!(/\b(func(tion)?|fn)\b/i, '*')
    gsub!(/\b(hyper)\b/i, '%')
    self
  end

  def lower_to_upper
    doubles = [
      [',', '<'], ['.', '>'], ['/', '?'], [';', ':'], ["'", '"'],
      ['[', '{'], [']', '}'], ['\\', '|'], ['-', '_'], ['=', '+']
    ]
    lowers = doubles.map { |d| d[0] }
    uppers = doubles.map { |d| d[1] }
    lowers.include?(self) ? uppers[lowers.index(self)] : self
  end

  def upper?
    %w(< > ? : " { } | ! @ # $ % ^ & * \( \) _ +).include?(self)
  end

  def clean_combo!
    replace clean_combo
  end

  def to_mod
    characters = {
      '^' => '⌃',
      '~' => '⌥',
      '$' => '⇧',
      '@' => '⌘',
      '*' => 'Fn',
      '%' => 'Hyper'
    }
    characters.key?(self) ? characters[self] : self
  end

  def mod_to_ent(use_symbol)
    entities = {
      '⌃' => '&#8963;',
      '⌥' => '&#8997;',
      '⇧' => '&#8679;',
      '⌘' => '&#8984;',
      'Fn' => 'Fn',
      'Hyper' => 'Hyper'
    }
    names = {
      '⌃' => 'Control',
      '⌥' => 'Option',
      '⇧' => 'Shift',
      '⌘' => 'Command',
      'Fn' => 'Function',
      'Hyper' => 'Hyper (Control+Option+Shift+Command)'
    }
    if entities.key?(self)
      use_symbol ? entities[self] : names[self]
    else
      self
    end
  end

  def mod_to_title
    entities = {
      '⌃' => 'Control',
      '⌥' => 'Option',
      '⇧' => 'Shift',
      '⌘' => 'Command',
      'Fn' => 'Function',
      'Hyper' => 'Hyper (Control+Option+Shift+Command)'
    }
    entities.key?(self) ? entities[self] : self
  end

  def clarify_characters
    unclear = {
      ',' => 'Comma (,)',
      '.' => 'Period (.)',
      ';' => 'Semicolon (;)',
      ':' => 'Colon (:)',
      '`' => 'Backtick (`)',
      '-' => 'Minus Sign (-)',
      '+' => 'Plus Sign (+)',
      '=' => 'Equals Sign (=)',
      '_' => 'Underscore (_)',
      '~' => 'Tilde (~)',
      '\\' => 'Backslash (\\)',
      '|' => 'Pipe (|)',
      '←' => 'Left Arrow (←)',
      '→' => 'Right Arrow (→)',
      '↑' => 'Up Arrow (↑)',
      '↓' => 'Down Arrow (↓)'
    }
    unclear.fetch(self, self)
  end

  def name_to_ent(use_symbol)
    k = case strip.downcase
        when /^f(\d{1,2})$/
          num = Regexp.last_match(1)
          ["F#{num}", "F#{num}", "F#{num} Key"]
        when /^apple$/
          ['Apple', '&#63743;', 'Apple menu']
        when /^tab$/
          ['', '&#8677;', 'Tab Key']
        when /^caps(lock)?$/
          ['Caps Lock', '&#8682;', 'Caps Lock Key']
        when /^eject$/
          ['Eject', '&#9167;', 'Eject Key']
        when /^return$/
          ['Return', '&#9166;', 'Return Key']
        when /^enter$/
          ['Enter', '&#8996;', 'Enter (Fn Return) Key']
        when /^(del(ete)?|back(space)?)$/
          ['Del', '&#9003;', 'Delete']
        when /^fwddel(ete)?$/
          ['Fwd Del', '&#8998;', 'Forward Delete (Fn Delete)']
        when /^(esc(ape)?)$/
          ['Esc', '&#9099;', 'Escape Key']
        when /^(right|rt)$/
          ['Right Arrow', '&#8594;', 'Right Arrow Key']
        when /^(left|lt)$/
          ['Left Arrow', '&#8592;', 'Left Arrow Key']
        when /^up$/
          ['Up Arrow', '&#8593;', 'Up Arrow Key']
        when /^(down|dn)$/
          ['Down Arrow', '&#8595;', 'Down Arrow Key']
        when /^p(age|g)up$/
          ['PgUp', '&#8670;', 'Page Up Key']
        when /^p(age|g)d(ow)?n$/
          ['PgDn', '&#8671;', 'Page Down Key']
        when /^home$/
          ['Home', '&#8598;', 'Home Key']
        when /^end$/
          ['End', '&#8600;', 'End Key']
        when /^semi(colon)$/
          ['Semicolon', ';', 'Semicolon']
        when /^(single([- ]?quote?)?|quote?)$/
          ['Single Quote', "'", 'Single Quote']
        when /^double([- ]?quote?)?$/
          ['Double Quote', '"', 'Double Quote']
        when /^click$/
          ['click', '<i class="fas fa-mouse-pointer"></i>', 'left click']
        when /^hyper/i
          ['Hyper', 'Hyper', 'Hyper (Control+Option+Shift+Command)']
        else
          [self, self, capitalize]
        end
    use_symbol ? [k[1], k[2]] : [k[0], k[2]]
  end
end

def render_kbd(markup, use_key_symbol: true, use_mod_symbol: true, use_plus: false)
  combos = []

  markup.split(%r{ / }).each do |combo|
    mods = []
    key = ''
    combo.clean_combo!
    combo.strip.split(//).each do |char|
      next if char == ' '
      case char
      when /[⌃⇧⌥⌘]/
        mods << char
      when /[*\^$@~%]/
        mods << char.to_mod
      else
        key << char
      end
    end
    mods = sort_mods(mods)
    title = ''
    if key.length == 1
      if mods.empty? && (key =~ /[A-Z]/ || key.upper?)
        mods << '$'.to_mod
      end
      key = key.lower_to_upper if mods.include?('$'.to_mod)
      key = key.upcase
      title = key.clarify_characters
    elsif mods.include?('$'.to_mod)
      key = key.lower_to_upper
    end
    key.gsub!(/"/, '&quot;')
    combos << { mods: mods, key: key, title: title }
  end

  outputs = combos.map do |combo|
    next if combo[:mods].empty? && combo[:key].empty?
    kbds = []
    title = []

    combo[:mods].each do |mod|
      mod_class = use_mod_symbol ? 'mod symbol' : 'mod'
      kbds << %(<kbd class="#{mod_class}">#{mod.mod_to_ent(use_mod_symbol)}</kbd>)
      title << mod.mod_to_title
    end

    unless combo[:key].empty?
      key, keytitle = combo[:key].name_to_ent(use_key_symbol)
      key_class = use_key_symbol ? 'key symbol' : 'key'
      keytitle = keytitle.clarify_characters if keytitle.length == 1
      kbds << %(<kbd class="#{key_class}">#{key}</kbd>)
      title << keytitle
    end

    kbd = if use_mod_symbol
            use_plus ? kbds.join('<span class="keycombo combiner">+</span>') : kbds.join
          else
            kbds.join('-')
          end
    span_class = "keycombo #{use_mod_symbol && !use_plus ? 'combined' : 'separated'}"
    %(<span class="#{span_class}" title="#{title.join('-')}">#{kbd}</span>)
  end.compact

  outputs.join('<span class="keycombo separator">/</span>')
end

def sort_mods(mods)
  order = ['Fn', '⌃', '⌥', '⇧', '⌘']
  mods.uniq.sort { |a, b| order.index(a) < order.index(b) ? -1 : 1 }
end

result = text.gsub(/\{%\s*kbd\s+([^%]+)%\}/) do
  markup = Regexp.last_match(1)
  render_kbd(markup)
end

print result

