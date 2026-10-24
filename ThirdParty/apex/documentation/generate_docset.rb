#!/usr/bin/env ruby
# Generate Dash docset for Apex documentation
#
# Usage:
#   ./generate_docset.rb [single|multi]
#
#   single - Generate single-page CLI options docset (ApexCLI.docset)
#   multi  - Generate multi-page docset from wiki files (Apex.docset, default)

require 'fileutils'
require 'yaml'
require 'uri'

begin
  require 'rouge'
  ROUGE_AVAILABLE = true
  puts "Rouge loaded successfully (version: #{Rouge::VERSION rescue 'unknown'})"
rescue LoadError => e
  ROUGE_AVAILABLE = false
  puts "Warning: Rouge gem not found. Install with: gem install rouge"
  puts "Error: #{e.message}" if e.message
  puts "Syntax highlighting will be disabled."
rescue => e
  ROUGE_AVAILABLE = false
  puts "Warning: Error loading Rouge: #{e.class}: #{e.message}"
  puts "Syntax highlighting will be disabled."
end

# Only require sqlite3 for multi-page docset
def require_sqlite3
  require 'sqlite3'
rescue LoadError
  puts "Error: sqlite3 gem is required for multi-page docset generation"
  puts "Install it with: gem install sqlite3"
  exit 1
end

MODE = ARGV[0] || 'multi'  # Default to multi-page docset
SCRIPT_DIR = File.expand_path(__dir__)
DOCS_DIR = File.join(SCRIPT_DIR, '..')
DOCSETS_DIR = File.join(DOCS_DIR, 'documentation', 'docsets')
WIKI_DIR = File.join(DOCS_DIR, 'documentation', 'apex.wiki')
MMD2CHEATSET = File.expand_path('~/Desktop/Code/mmd2cheatset/mmd2cheatset.rb')

# Find Apex binary - prioritize build/apex (most recent build)
def find_apex_binary
  # Prioritize build/apex if it exists (most recent build)
  build_apex = File.expand_path('../build/apex', __dir__)
  return build_apex if File.exist?(build_apex)

  # Try build-release (relative to repo root, not script dir)
  build_release_apex = File.expand_path('../build-release/apex', __dir__)
  return build_release_apex if File.exist?(build_release_apex)

  # Try other common build directories
  ['../build-debug/apex'].each do |path|
    full_path = File.expand_path(path, __dir__)
    return full_path if File.exist?(full_path)
  end

  # Fall back to system-installed apex
  system_apex = `which apex 2>/dev/null`.strip
  return system_apex if system_apex != '' && File.exist?(system_apex)

  nil
end

APEX_BIN = find_apex_binary

def generate_single_page_docset
  puts "Generating single-page docset using mmd2cheatset..."

  # Ensure output directory exists
  FileUtils.mkdir_p(DOCSETS_DIR)

  # Change to docsets directory for output
  original_dir = Dir.pwd
  Dir.chdir(DOCSETS_DIR)

  # Create a cheatsheet markdown file for command-line options
  cheatsheet_md = <<~MARKDOWN
    title: Apex Command Line Options
    name: ApexCLI
    keyword: apex

    Complete reference for all Apex command-line flags and options.

    | name | command | note |
    |------|---------|------|
    | Help | -h, --help | Display help message and exit |
    | Version | -v, --version | Display version information and exit |
    | Progress | --[no-]progress | Show progress indicator during processing |
    | Combine | --combine | Concatenate Markdown files into a single stream |
    | MMD Merge | --mmd-merge | Merge files from MultiMarkdown index files |
    | Mode | -m, --mode MODE | Set processor mode (commonmark, gfm, mmd, kramdown, unified) |
    | Output | -o, --output FILE | Write output to a file instead of stdout |
    | Standalone | -s, --standalone | Generate complete HTML document |
    | Style | --style FILE, --css FILE | Link to a CSS file in document head |
    | Embed CSS | --embed-css | Embed CSS file contents into style tag |
    | Script | --script VALUE | Inject script tags (mermaid, mathjax, katex, etc.) |
    | Title | --title TITLE | Set the document title |
    | Pretty | --pretty | Pretty-print HTML with indentation |
    | ARIA | --aria | Add ARIA labels and accessibility attributes |
    | Plugins | --plugins, --no-plugins | Enable or disable plugin processing |
    | List Plugins | --list-plugins | List installed and available plugins |
    | Install Plugin | --install-plugin ID-or-URL | Install a plugin by ID or URL |
    | Uninstall Plugin | --uninstall-plugin ID | Uninstall a locally installed plugin |
    | ID Format | --id-format FORMAT | Set header ID generation format (gfm, mmd, kramdown) |
    | No IDs | --no-ids | Disable automatic header ID generation |
    | Header Anchors | --header-anchors | Generate anchor tags instead of id attributes |
    | Relaxed Tables | --relaxed-tables | Enable relaxed table parsing |
    | No Relaxed Tables | --no-relaxed-tables | Disable relaxed table parsing |
    | Captions | --captions POSITION | Control table caption position (above, below) |
    | No Tables | --no-tables | Disable table support entirely |
    | Alpha Lists | --[no-]alpha-lists | Control alphabetic list markers (a., b., c.) |
    | Mixed Lists | --[no-]mixed-lists | Control mixed list marker types |
    | No Footnotes | --no-footnotes | Disable footnote support |
    | No Smart | --no-smart | Disable smart typography |
    | No Math | --no-math | Disable math support |
    | Autolink | --[no-]autolink | Control automatic linking of URLs and email addresses |
    | Obfuscate Emails | --obfuscate-emails | Obfuscate email links by hex-encoding |
    | Includes | --includes, --no-includes | Enable or disable file inclusion features |
    | Embed Images | --embed-images | Embed local images as base64 data URLs |
    | Base Dir | --base-dir DIR | Set base directory for resolving relative paths |
    | Transforms | --[no-]transforms | Control metadata variable transforms |
    | Meta File | --meta-file FILE | Load metadata from external file |
    | Meta | --meta KEY=VALUE | Set metadata from command line |
    | Bibliography | --bibliography FILE | Specify bibliography file (BibTeX, CSL JSON, CSL YAML) |
    | CSL | --csl FILE | Specify Citation Style Language file |
    | No Bibliography | --no-bibliography | Suppress bibliography output |
    | Link Citations | --link-citations | Link citations to bibliography entries |
    | Show Tooltips | --show-tooltips | Enable tooltips on citations when hovering |
    | Indices | --indices | Enable index processing |
    | No Indices | --no-indices | Disable index processing entirely |
    | No Index | --no-index | Suppress index generation while creating markers |
    | Hardbreaks | --hardbreaks | Treat newlines as hard breaks (GFM style) |
    | Sup Sub | --[no-]sup-sub | Control MultiMarkdown-style superscript and subscript |
    | Divs | --[no-]divs | Control Pandoc fenced divs syntax |
    | Spans | --[no-]spans | Control Pandoc-style bracketed spans |
    | Emoji | --[no-]emoji | Enable/disable :name: emoji replacement |
    | Emoji Autocorrect | --[no-]emoji-autocorrect | Control emoji name autocorrect |
    | Unsafe | --[no-]unsafe | Control whether raw HTML is allowed |
    | Wikilinks | --[no-]wikilinks | Control wiki link syntax |
    | Wikilink Space | --wikilink-space MODE | Control how spaces in wiki links are handled |
    | Wikilink Extension | --wikilink-extension EXT | Add file extension to wiki link URLs |
    | Accept | --accept | Accept all Critic Markup changes |
    | Reject | --reject | Reject all Critic Markup changes |
    [Command Line Options]

    ---

    For complete documentation, see the Apex wiki at https://github.com/ApexMarkdown/apex/wiki
  MARKDOWN

  File.write('Apex_Command_Line_Options.md', cheatsheet_md)

  if File.exist?(MMD2CHEATSET)
    system("#{MMD2CHEATSET} Apex_Command_Line_Options.md")
    FileUtils.rm('Apex_Command_Line_Options.md')
    puts "\nSingle-page docset generated successfully!"
    puts "Docset location: #{File.join(DOCSETS_DIR, "ApexCLI.docset")}"
  else
    puts "Error: mmd2cheatset.rb not found at #{MMD2CHEATSET}"
    puts "Please check the path or install mmd2cheatset"
    Dir.chdir(original_dir)
    exit 1
  end

  # Return to original directory
  Dir.chdir(original_dir)
end

def highlight_code_blocks(html_content)
  return html_content unless ROUGE_AVAILABLE

  code_block_count = 0
  highlighted_count = 0
  error_count = 0

  begin
    # Find all code blocks - handle various formats
    # Apex outputs: <pre lang="language"><code>...</code></pre>
    # Or: <pre><code class="language-xxx">...</code></pre>
    # Use non-greedy matching and ensure we match complete code blocks
    # Match <pre> tag (with optional lang attribute), then <code> tag (with optional class), then content, then closing tags
    result = html_content.gsub(/<pre(\s+lang=["']([^"']+)["'])?[^>]*>\s*<code(\s+class=["'](?:language-)?([^"'\s]+)["'])?[^>]*>([\s\S]*?)<\/code>\s*<\/pre>/i) do |match|
      code_block_count += 1
      lang = $2 || $4  # Get lang from <pre lang="..."> ($2) or class from <code class="..."> ($4)
      code = $5

      # Skip if code is empty or suspiciously large (might be a regex error)
      if code.nil? || code.empty? || code.length > 100000
        match
      else
        begin
          # Unescape HTML entities
          code = code.gsub(/&lt;/, '<').gsub(/&gt;/, '>').gsub(/&amp;/, '&').gsub(/&quot;/, '"')

          if lang && !lang.empty?
            lang_normalized = lang.downcase
            # Handle common language aliases
            lang_aliases = {
              'yml' => 'yaml',
              'md' => 'markdown',
              'mkd' => 'markdown',
              'mkdn' => 'markdown',
              'mdown' => 'markdown'
            }
            lang_normalized = lang_aliases[lang_normalized] || lang_normalized

            lexer = Rouge::Lexer.find(lang_normalized)
            if lexer.nil?
              puts "  Warning: No lexer found for language '#{lang}' (normalized: '#{lang_normalized}'), using PlainText"
              lexer = Rouge::Lexers::PlainText
            end
          else
            lexer = Rouge::Lexers::PlainText
          end

          formatter = Rouge::Formatters::HTML.new
          highlighted = formatter.format(lexer.lex(code))
          highlighted_count += 1

          # Return highlighted code with proper classes
          "<pre><code class=\"highlight #{lang ? "language-#{lang}" : ''}\">#{highlighted}</code></pre>"
        rescue => e
          error_count += 1
          # If highlighting fails, return original
          puts "  Warning: Failed to highlight code block (lang: #{lang || 'none'}): #{e.class}: #{e.message}"
          puts "  Backtrace: #{e.backtrace.first(2).join(' | ')}" if $DEBUG
          match
        end
      end
    end

    if code_block_count > 0
      puts "  Highlighted #{highlighted_count}/#{code_block_count} code blocks"
      puts "  Errors: #{error_count}" if error_count > 0
    end

    result
  rescue => e
    puts "Warning: Error in highlight_code_blocks: #{e.class}: #{e.message}"
    puts "Backtrace: #{e.backtrace.first(3).join("\n")}" if $DEBUG
    puts "Returning original HTML without highlighting"
    html_content
  end
end

# Rouge CSS is now included in shared_styles.css

def extract_headers(html_content)
  headers = []
  # Match headers with IDs, handling multi-line formatting
  html_content.scan(/<h([1-6])[^>]*id=["']([^"']+)["'][^>]*>([\s\S]*?)<\/h[1-6]>/i) do |level, id, text|
    # Remove HTML tags and normalize whitespace
    text = text.gsub(/<[^>]+>/, '').gsub(/\s+/, ' ').strip
    headers << { level: level.to_i, id: id, text: text } unless text.empty?
  end
  headers
end

def parse_sidebar_toc
  sidebar_file = File.join(WIKI_DIR, '_Sidebar.md')
  toc_html = '<nav class="main-toc">'
  toc_html += '<ul>'

  # Always add Home first
  toc_html += '<li><a href="Home.html">Home</a></li>'

  if File.exist?(sidebar_file)
    sidebar_content = File.read(sidebar_file)
    # Parse markdown links: [Text](PageName) or [Text](Page-Name)
    sidebar_content.scan(/\[([^\]]+)\]\(([^)]+)\)/) do |text, link|
      # Remove .md extension if present, add .html
      page_name = link.gsub(/\.md$/, '')
      # Skip if it's Home (already added)
      next if page_name.downcase == 'home'
      toc_html += "<li><a href=\"#{page_name}.html\">#{text}</a></li>"
    end
  end

  toc_html += '</ul>'
  toc_html += '</nav>'
  toc_html
end

def generate_page_toc(headers)
  return '' if headers.empty?

  toc_html = '<nav class="page-toc">'
  toc_html += '<ul>'
  stack = []  # Track open list items that need closing

  headers.each_with_index do |header, idx|
    level = header[:level]
    next_header = headers[idx + 1]
    next_level = next_header ? next_header[:level] : 1

    # Close lists if we're going up in level
    while !stack.empty? && stack.last >= level
      toc_html += '</ul></li>'
      stack.pop
    end

    # If next item is deeper, this item will have children
    if next_level > level
      toc_html += "<li><a href=\"##{header[:id]}\">#{header[:text]}</a><ul>"
      stack.push(level)
    else
      toc_html += "<li><a href=\"##{header[:id]}\">#{header[:text]}</a></li>"
    end
  end

  # Close all remaining lists
  while !stack.empty?
    toc_html += '</ul></li>'
    stack.pop
  end

  toc_html += '</ul>'
  toc_html += '</nav>'
  toc_html
end

def inject_toc_into_html(html_content, main_toc_html, page_toc_html)
  # Load shared CSS
  shared_css_file = File.join(SCRIPT_DIR, 'shared_styles.css')
  shared_css = File.exist?(shared_css_file) ? File.read(shared_css_file) : ''
  toc_css = "<style>\n#{shared_css}\n</style>"

  # Inject CSS into head (Rouge CSS is included in shared_styles.css)
  combined_css = toc_css
  if html_content =~ /(<\/head>)/i
    html_content = html_content.sub(/(<\/head>)/i, "#{combined_css}\\1")
  elsif html_content =~ /(<\/style>)/i
    html_content = html_content.sub(/(<\/style>)/i, "\\1\n#{combined_css}")
  end

  # Inject hamburger menu and mobile overlay
  hamburger_html = <<~HTML
    <button class="hamburger-menu" id="hamburger-menu" aria-label="Toggle navigation"></button>
    <div class="mobile-menu-overlay" id="mobile-menu-overlay"></div>
  HTML

  # Inject main TOC into body (left sidebar)
  if html_content =~ /(<body[^>]*>)/i
    html_content = html_content.sub(/(<body[^>]*>)/i, "\\1\n#{hamburger_html}\n#{main_toc_html}")
  else
    puts "Warning: Could not find <body> tag to inject main TOC"
  end

  # Inject page TOC at top of content (after first h1, or at start of body if no h1)
  # Handle multi-line h1 tags (from pretty printing)
  # Add ID to page TOC for scroll detection
  unless page_toc_html.empty?
    page_toc_with_id = page_toc_html.sub(/<nav class="page-toc">/, '<nav class="page-toc" id="page-toc-top">')

    if html_content =~ /<h1[^>]*>[\s\S]*?<\/h1>/i
      html_content = html_content.sub(/(<h1[^>]*>[\s\S]*?<\/h1>)/i, "\\1\n#{page_toc_with_id}")
    elsif html_content =~ /(<body[^>]*>)/i
      # If no h1, inject TOC right after body tag
      html_content = html_content.sub(/(<body[^>]*>)/i, "\\1\n#{page_toc_with_id}")
    else
      puts "Warning: Could not find <h1> or <body> tag to inject page TOC"
    end
  end

  # Add floating TOC HTML structure
  floating_toc_html = <<~HTML
    <div class="floating-toc" id="floating-toc">
      <div class="floating-toc-container">
        <div class="floating-toc-header">
          <span>Table of Contents 🔻</span>
        </div>
        <div class="floating-toc-content" id="floating-toc-content">
          <!-- Content will be populated by JavaScript -->
        </div>
      </div>
    </div>
  HTML

  # Inject floating TOC after body tag
  if html_content =~ /(<body[^>]*>)/i
    html_content = html_content.sub(/(<body[^>]*>)/i, "\\1\n#{floating_toc_html}")
  end

  # Add JavaScript for floating TOC
  floating_toc_js = <<~JS
    <script>
      (function() {
        // Clone the page TOC for floating TOC
        function initFloatingTOC() {
          var pageTOC = document.getElementById('page-toc-top');
          var floatingTOCContent = document.getElementById('floating-toc-content');
          var floatingTOC = document.getElementById('floating-toc');

          if (!pageTOC || !floatingTOCContent || !floatingTOC) return;

          // Clone the TOC structure
          var tocClone = pageTOC.cloneNode(true);
          tocClone.id = 'floating-toc-clone';
          floatingTOCContent.appendChild(tocClone);

          // Update all links to use smooth scrolling
          var allTOCLinks = document.querySelectorAll('.page-toc a, .floating-toc-content a');
          allTOCLinks.forEach(function(link) {
            link.addEventListener('click', function(e) {
              var href = this.getAttribute('href');
              if (href && href.startsWith('#')) {
                e.preventDefault();
                var targetId = href.substring(1);
                var targetElement = document.getElementById(targetId);
                if (targetElement) {
                  var offset = 20; // Offset from top

                  // Function to calculate absolute position from document top
                  function getAbsoluteTop(element) {
                    var top = 0;
                    while (element) {
                      top += element.offsetTop;
                      element = element.offsetParent;
                    }
                    return top;
                  }

                  var absoluteTop = getAbsoluteTop(targetElement);
                  var scrollTop = window.pageYOffset || document.documentElement.scrollTop;
                  var offsetPosition = absoluteTop - offset;

                  // Only scroll if we're not already at the target position
                  if (Math.abs(scrollTop - offsetPosition) > 10) {
                    window.scrollTo({
                      top: Math.max(0, offsetPosition),
                      behavior: 'smooth'
                    });
                  }

                  // Update URL hash without triggering scroll
                  if (history.pushState) {
                    history.pushState(null, null, href);
                  }
                }
              }
            });
          });

          // Handle scroll to show/hide floating TOC
          var tocTop = pageTOC.getBoundingClientRect().top + window.pageYOffset;
          var tocBottom = tocTop + pageTOC.offsetHeight;

          function updateFloatingTOC() {
            var scrollY = window.pageYOffset || document.documentElement.scrollTop;

            if (scrollY > tocBottom) {
              floatingTOC.classList.add('visible');
            } else {
              floatingTOC.classList.remove('visible');
            }
          }

          // Throttle scroll events
          var ticking = false;
          window.addEventListener('scroll', function() {
            if (!ticking) {
              window.requestAnimationFrame(function() {
                updateFloatingTOC();
                ticking = false;
              });
              ticking = true;
            }
          });

          // Initial check
          updateFloatingTOC();
        }

        // Initialize when DOM is ready
        if (document.readyState === 'loading') {
          document.addEventListener('DOMContentLoaded', initFloatingTOC);
        } else {
          initFloatingTOC();
        }
      })();
    </script>
  JS

  # Load shared JavaScript
  shared_js_file = File.join(SCRIPT_DIR, 'shared_scripts.js')
  shared_js = File.exist?(shared_js_file) ? File.read(shared_js_file) : ''

  # Combine shared JS and floating TOC JS
  combined_js = "<script>\n#{shared_js}\n</script>\n#{floating_toc_js}"

  # Inject JavaScript before closing body tag
  if html_content =~ /(<\/body>)/i
    html_content = html_content.sub(/(<\/body>)/i, "#{combined_js}\\1")
  end

  html_content
end

def parse_footer
  footer_file = File.join(WIKI_DIR, '_Footer.md')
  return '' unless File.exist?(footer_file)

  # Convert markdown to HTML using Apex
  footer_html_content = `#{APEX_BIN} "#{footer_file}" 2>/dev/null`

  if $?.success? && !footer_html_content.empty?
    footer_html = '<footer class="page-footer">'
    footer_html += footer_html_content.strip
    footer_html += '</footer>'
    footer_html
  else
    # Fallback: simple text conversion
    footer_content = File.read(footer_file)
    footer_html = '<footer class="page-footer">'
    footer_html += '<p>' + footer_content.strip.gsub(/\n/, '<br>') + '</p>'
    footer_html += '</footer>'
    footer_html
  end
end

def inject_footer_into_html(html_content, footer_html)
  return html_content if footer_html.empty?

  # Inject footer before closing body tag
  if html_content =~ /(<\/body>)/i
    html_content = html_content.sub(/(<\/body>)/i, "#{footer_html}\\1")
  end

  html_content
end

def fix_links_in_html(html_content, available_files)
  # Create a mapping of page names to HTML files
  file_map = {}
  available_files.each do |file|
    basename = File.basename(file, '.html')
    file_map[basename] = "#{basename}.html"
    # Also map with different cases/spaces
    file_map[basename.gsub('-', ' ')] = "#{basename}.html"
    file_map[basename.gsub('-', '_')] = "#{basename}.html"
  end

  # Fix relative links that don't have .html extension
  # Match href="PageName" or href="Page-Name" (but not external links, anchors, or already .html)
  html_content.gsub(/<a\s+([^>]*\s+)?href=["']([^"']+)["']([^>]*)>/i) do |match|
    attrs_before = $1 || ''
    href = $2
    attrs_after = $3 || ''

    # Skip if it's already an external link, anchor, or has extension
    if href =~ /^(https?:\/\/|mailto:|#|.*\.(html|md|pdf|png|jpg|jpeg|gif|svg|webp))/i
      match
    elsif file_map[href]
      # Found a matching file, add .html extension
      "<a #{attrs_before}href=\"#{file_map[href]}\"#{attrs_after}>"
    elsif file_map[href.gsub(/\s+/, '-')]
      # Try with spaces converted to dashes
      fixed_href = file_map[href.gsub(/\s+/, '-')]
      "<a #{attrs_before}href=\"#{fixed_href}\"#{attrs_after}>"
    else
      # Try to find a case-insensitive match
      found = available_files.find { |f| File.basename(f, '.html').downcase == href.downcase }
      if found
        "<a #{attrs_before}href=\"#{File.basename(found)}\"#{attrs_after}>"
      else
        match  # Keep original if no match found
      end
    end
  end
end

def clone_wiki
  wiki_url = 'https://github.com/ApexMarkdown/apex.wiki.git'
  puts "Cloning wiki from GitHub..."

  if File.exist?(WIKI_DIR)
    puts "Wiki directory already exists, removing..."
    FileUtils.rm_rf(WIKI_DIR)
  end

  success = system("git clone #{wiki_url} \"#{WIKI_DIR}\" 2>&1")
  unless success
    puts "Error: Failed to clone wiki from #{wiki_url}"
    exit 1
  end

  puts "Wiki cloned successfully"
end

def cleanup_wiki
  if File.exist?(WIKI_DIR)
    puts "\nCleaning up wiki clone..."
    FileUtils.rm_rf(WIKI_DIR)
    puts "Wiki clone removed"
  end
end

def generate_multi_page_docset
  require_sqlite3  # Only needed for multi-page mode

  puts "Generating multi-page docset from wiki files..."

  # Clone wiki if needed
  clone_wiki unless File.exist?(WIKI_DIR)

  unless File.exist?(WIKI_DIR)
    puts "Error: Wiki directory not found at #{WIKI_DIR}"
    exit 1
  end

  unless File.exist?(APEX_BIN)
    puts "Error: Apex binary not found at #{APEX_BIN}"
    puts "Please build Apex first: cd build-release && make"
    puts "Or ensure 'apex' is in your PATH"
    cleanup_wiki
    exit 1
  end

  # Ensure output directory exists
  FileUtils.mkdir_p(DOCSETS_DIR)

  docset_name = 'Apex.docset'
  docset_path = File.join(DOCSETS_DIR, docset_name)
  contents_path = File.join(docset_path, 'Contents')
  resources_path = File.join(contents_path, 'Resources')
  documents_path = File.join(resources_path, 'Documents')

  # Clean up existing docset
  FileUtils.rm_rf(docset_path) if File.exist?(docset_path)

  # Create directory structure
  FileUtils.mkdir_p(documents_path)

  # Get all markdown files from wiki (excluding special files)
  wiki_files = Dir.glob(File.join(WIKI_DIR, '*.md')).reject do |f|
    basename = File.basename(f)
    basename =~ /^(_|\.)/ || basename == 'commit_message.txt'
  end.sort

  # Ensure Home.md is first (it's the index file)
  home_file = wiki_files.find { |f| File.basename(f) == 'Home.md' }
  if home_file
    wiki_files.delete(home_file)
    wiki_files.unshift(home_file)
  end

  puts "Found #{wiki_files.length} wiki files to process..."

  # Process each wiki file - first pass: convert to HTML
  html_files = []
  wiki_files.each do |md_file|
    basename = File.basename(md_file, '.md')
    html_file = File.join(documents_path, "#{basename}.html")
    html_files << html_file

    puts "Processing #{basename}..."

    # Convert markdown to HTML using Apex
    html_content = `#{APEX_BIN} "#{md_file}" --standalone --pretty 2>/dev/null`

    if $?.success? && !html_content.empty?
      # Write HTML file (will fix links in second pass)
      File.write(html_file, html_content)
    else
      puts "  Warning: Failed to process #{basename}"
    end
  end

  # Generate main TOC from sidebar
  puts "\nGenerating main TOC from sidebar..."
  main_toc_html = parse_sidebar_toc
  puts "Main TOC generated (#{main_toc_html.length} chars)"
  if main_toc_html.length < 100
    puts "Warning: TOC seems too short, checking..."
    puts "First 200 chars: #{main_toc_html[0..200]}"
  end

  # Generate footer
  puts "Generating footer..."
  footer_html = parse_footer
  puts "Footer generated (#{footer_html.length} chars)"

  # Second pass: fix links in all HTML files and add TOCs
  puts "Fixing links and adding TOCs..."
  entries = []
  all_guides = []  # Collect all guide entries for TOC

  html_files.each do |html_file|
    next unless File.exist?(html_file)

    basename = File.basename(html_file, '.html')
    html_content = File.read(html_file)

    # Fix links to add .html extensions
    html_content = fix_links_in_html(html_content, html_files.map { |f| File.basename(f) })

    # Extract title from first h1 or use filename
    title_match = html_content.match(/<h1[^>]*>(.*?)<\/h1>/i)
    title = title_match ? title_match[1].gsub(/<[^>]+>/, '').strip : basename

    # Collect guide info for TOC
    all_guides << { basename: basename, title: title, path: "#{basename}.html" }

    # Extract headers for index and TOC
    headers = extract_headers(html_content)

    # Generate TOC for this page
    page_toc_html = generate_page_toc(headers)
    if page_toc_html.empty?
      puts "  No page TOC for #{basename} (no headers found)"
    else
      puts "  Generated page TOC for #{basename} (#{headers.length} headers)"
    end

    # Highlight code blocks before injecting TOC
    html_content = highlight_code_blocks(html_content)

    # Inject both TOCs and footer into HTML
    html_content = inject_toc_into_html(html_content, main_toc_html, page_toc_html)
    html_content = inject_footer_into_html(html_content, footer_html)

    # Write updated HTML file with TOCs and footer
    File.write(html_file, html_content)

    # Add main entry
    entries << {
      name: title,
      type: 'Guide',
      path: "#{basename}.html"
    }

    # Add header entries
    headers.each do |header|
      entries << {
        name: header[:text],
        type: 'Section',
        path: "#{basename}.html##{header[:id]}"
      }
    end
  end

  # Add table of contents to Home.html for Dash index
  home_html_file = html_files.find { |f| File.basename(f, '.html') == 'Home' }
  if home_html_file && File.exist?(home_html_file)
    puts "Adding table of contents to index page..."
    html_content = File.read(home_html_file)

    # Generate TOC HTML
    toc_html = "<nav class=\"dash-toc\">\n<h2>Documentation</h2>\n<ul>\n"
    all_guides.each do |guide|
      toc_html += "  <li><a href=\"#{guide[:path]}\">#{guide[:title]}</a></li>\n"
    end
    toc_html += "</ul>\n</nav>\n"

    # Insert TOC after the first h1 or at the beginning of body
    if html_content =~ /(<h1[^>]*>.*?<\/h1>)/i
      html_content = html_content.sub(/(<h1[^>]*>.*?<\/h1>)/i, "\\1\n#{toc_html}")
    elsif html_content =~ /(<body[^>]*>)/i
      html_content = html_content.sub(/(<body[^>]*>)/i, "\\1\n#{toc_html}")
    end

    File.write(home_html_file, html_content)
  end

  # Determine index file
  index_file = if entries.any? { |e| e[:path] == 'Home.html' }
    'Home.html'
  elsif entries.first
    entries.first[:path]
  else
    'index.html'
  end

  # Create Info.plist
  info_plist = <<~PLIST
    <?xml version="1.0" encoding="UTF-8"?>
    <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
    <plist version="1.0">
    <dict>
      <key>CFBundleIdentifier</key>
      <string>apex</string>
      <key>CFBundleName</key>
      <string>Apex</string>
      <key>DocSetPlatformFamily</key>
      <string>apex</string>
      <key>isDashDocset</key>
      <true/>
      <key>dashIndexFilePath</key>
      <string>#{index_file}</string>
      <key>DashDocSetFamily</key>
      <string>dashtoc</string>
      <key>DashDocSetPluginKeyword</key>
      <string>apex</string>
      <key>DashDocSetFallbackURL</key>
      <string>#{index_file}</string>
      <key>DashDocSetDeclaredInStyle</key>
      <string>originalName</string>
    </dict>
    </plist>
  PLIST

  File.write(File.join(contents_path, 'Info.plist'), info_plist)

  # Create SQLite index
  db_path = File.join(resources_path, 'docSet.dsidx')
  # Open database with busy timeout and retry logic
  db = nil
  max_retries = 5
  retry_count = 0

  begin
    db = SQLite3::Database.new(db_path)
    db.busy_timeout = 5000  # Wait up to 5 seconds for database to be available

    db.execute <<~SQL
      CREATE TABLE IF NOT EXISTS searchIndex(
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT,
        type TEXT,
        path TEXT
      );
      CREATE UNIQUE INDEX IF NOT EXISTS anchor ON searchIndex(name, type, path);
    SQL

    # Clear existing entries if regenerating
    db.execute("DELETE FROM searchIndex")

    # Use a transaction for better performance and atomicity
    db.transaction do
      entries.each do |entry|
        db.execute(
          "INSERT OR IGNORE INTO searchIndex(name, type, path) VALUES(?, ?, ?)",
          [entry[:name], entry[:type], entry[:path]]
        )
      end
    end
  rescue SQLite3::BusyException => e
    retry_count += 1
    if retry_count < max_retries
      puts "Database busy, retrying (#{retry_count}/#{max_retries})..."
      sleep(0.5 * retry_count)  # Exponential backoff
      db.close if db
      retry
    else
      puts "\nError: Database is locked after #{max_retries} retries."
      puts "Please close Dash if it's open with this docset, then try again."
      raise
    end
  ensure
    db.close if db
  end

  puts "\nMulti-page docset generated successfully!"
  puts "Docset location: #{docset_path}"
  puts "Total entries: #{entries.length}"

  # Clean up wiki clone
  cleanup_wiki
end

case MODE
when 'single'
  generate_single_page_docset
when 'multi'
  generate_multi_page_docset
else
  puts "Unknown mode: #{MODE}"
  puts "Usage: #{$0} [single|multi]"
  exit 1
end
