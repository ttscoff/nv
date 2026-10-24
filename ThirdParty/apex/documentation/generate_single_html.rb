#!/usr/bin/env ruby
# Generate single HTML file with all wiki pages
# Uses JavaScript to show/hide pages based on sidebar selection

require 'fileutils'

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

SCRIPT_DIR = File.expand_path(__dir__)
DOCS_DIR = File.join(SCRIPT_DIR, '..')
HTML_DIR = File.join(DOCS_DIR, 'documentation', 'html')
WIKI_DIR = File.join(DOCS_DIR, 'documentation', 'apex.wiki')

# Find Apex binary
def find_apex_binary
  # Prioritize build/apex if it exists (most recent build)
  build_apex = File.expand_path('../build/apex', __dir__)
  return build_apex if File.exist?(build_apex)

  # Try build-release (relative to repo root, not script dir)
  build_release_apex = File.expand_path('../build-release/apex', __dir__)
  return build_release_apex if File.exist?(build_release_apex)

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

def parse_sidebar
  sidebar_file = File.join(WIKI_DIR, '_Sidebar.md')
  pages = []

  # Always add Home first
  pages << { name: 'Home', title: 'Home', file: 'Home.md' }

  if File.exist?(sidebar_file)
    sidebar_content = File.read(sidebar_file)
    sidebar_content.scan(/\[([^\]]+)\]\(([^)]+)\)/) do |text, link|
      page_name = link.gsub(/\.md$/, '')
      next if page_name.downcase == 'home'
      pages << { name: page_name, title: text, file: "#{page_name}.md" }
    end
  end

  pages
end

def parse_footer
  footer_file = File.join(WIKI_DIR, '_Footer.md')
  return '' unless File.exist?(footer_file)

  footer_content = `#{APEX_BIN} "#{footer_file}" 2>/dev/null`
  if $?.success? && !footer_content.empty?
    footer_content.strip
  else
    footer_text = File.read(footer_file).strip
    "<p>#{footer_text.gsub(/\n/, '<br>')}</p>"
  end
end

# Rouge CSS is now included in shared_styles.css

def generate_css
  # Load shared CSS
  shared_css_file = File.join(SCRIPT_DIR, 'shared_styles.css')
  shared_css = File.exist?(shared_css_file) ? File.read(shared_css_file) : ''

  # Additional styles specific to single-page HTML
  additional_css = <<~CSS
    <style>
      * {
        margin: 0;
        padding: 0;
        box-sizing: border-box;
      }
      #{shared_css}
      .content {
        padding: 2rem;
        max-width: 900px;
      }
      .page {
        display: none;
      }
      .page.active {
        display: block;
      }
      h1, h2, h3, h4, h5, h6 {
        margin-top: 1.5em;
        margin-bottom: 0.5em;
      }
      h1 {
        font-size: 2em;
        border-bottom: 2px solid #eee;
        padding-bottom: 0.3em;
      }
      h2 {
        font-size: 1.5em;
        border-bottom: 1px solid #eee;
        padding-bottom: 0.3em;
      }
      blockquote {
        border-left: 4px solid #ddd;
        margin: 0;
        padding-left: 1rem;
        color: #666;
      }
      table {
        border-collapse: collapse;
        width: 100%;
      }
      th, td {
        border: 1px solid #ddd;
        padding: 0.5rem;
      }
      th {
        background: #f5f5f5;
      }
    </style>
  CSS
end

def generate_javascript
  # Load shared JavaScript
  shared_js_file = File.join(SCRIPT_DIR, 'shared_scripts.js')
  shared_js = File.exist?(shared_js_file) ? File.read(shared_js_file) : ''

  <<~JS
    <script>
      #{shared_js}
      function showPage(pageId) {
        // Hide all pages
        var pages = document.querySelectorAll('.page');
        pages.forEach(function(page) {
          page.classList.remove('active');
        });

        // Show selected page
        var selectedPage = document.getElementById('page-' + pageId);
        if (selectedPage) {
          selectedPage.classList.add('active');
        }

        // Update active link
        var links = document.querySelectorAll('.sidebar a');
        links.forEach(function(link) {
          link.classList.remove('active');
        });
        var activeLink = document.querySelector('.sidebar a[data-page="' + pageId + '"]');
        if (activeLink) {
          activeLink.classList.add('active');
        }

        // Scroll to top
        window.scrollTo(0, 0);

        // Update URL hash without triggering scroll
        if (history.pushState) {
          history.pushState(null, null, '#' + pageId);
        } else {
          window.location.hash = pageId;
        }

      }

      // Initialize floating TOC for a page
      function initFloatingTOC(pageElement) {
        if (!pageElement) return;

        var pageTOC = pageElement.querySelector('.page-toc');
        if (!pageTOC) return;

        // Add ID to page TOC if it doesn't have one
        if (!pageTOC.id) {
          pageTOC.id = 'page-toc-top-' + pageElement.id;
        }

        // Find or create floating TOC for this page
        var floatingTOC = pageElement.querySelector('.floating-toc');
        if (!floatingTOC) {
          floatingTOC = document.createElement('div');
          floatingTOC.className = 'floating-toc';
          floatingTOC.id = 'floating-toc-' + pageElement.id;
          floatingTOC.innerHTML = '<div class="floating-toc-container"><div class="floating-toc-header"><span>Table of Contents 🔻</span></div><div class="floating-toc-content" id="floating-toc-content-' + pageElement.id + '"></div></div>';
          pageElement.insertBefore(floatingTOC, pageElement.firstChild);
        }

        var floatingTOCContent = floatingTOC.querySelector('.floating-toc-content');
        if (!floatingTOCContent) return;

        // Clone the TOC structure
        var tocClone = pageTOC.cloneNode(true);
        tocClone.id = 'floating-toc-clone-' + pageElement.id;
        floatingTOCContent.innerHTML = '';
        floatingTOCContent.appendChild(tocClone);

        // Update all links to use smooth scrolling
        var allTOCLinks = pageElement.querySelectorAll('.page-toc a, .floating-toc-content a');
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

                // Ensure the target element's page is visible
                var targetPage = targetElement.closest('.page');
                if (targetPage && !targetPage.classList.contains('active')) {
                  // If target is in a different page, switch to that page first
                  var pageId = targetPage.id.replace('page-', '');
                  if (typeof showPage === 'function') {
                    showPage(pageId);
                    // Wait for page to be visible, then scroll
                    setTimeout(function() {
                      var absoluteTop = getAbsoluteTop(targetElement);
                      window.scrollTo({
                        top: Math.max(0, absoluteTop - offset),
                        behavior: 'smooth'
                      });
                    }, 150);
                  }
                } else {
                  // Target is in current page, calculate and scroll
                  var absoluteTop = getAbsoluteTop(targetElement);
                  var scrollTop = window.pageYOffset || document.documentElement.scrollTop;
                  var offsetPosition = absoluteTop - offset;

                  if (Math.abs(scrollTop - offsetPosition) > 10) {
                    window.scrollTo({
                      top: Math.max(0, offsetPosition),
                      behavior: 'smooth'
                    });
                  }
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
          if (!pageElement.classList.contains('active')) {
            floatingTOC.classList.remove('visible');
            return;
          }

          var scrollY = window.pageYOffset || document.documentElement.scrollTop;

          if (scrollY > tocBottom) {
            floatingTOC.classList.add('visible');
          } else {
            floatingTOC.classList.remove('visible');
          }
        }

        // Throttle scroll events
        var ticking = false;
        function handleScroll() {
          if (!ticking) {
            window.requestAnimationFrame(function() {
              updateFloatingTOC();
              ticking = false;
            });
            ticking = true;
          }
        }

        window.addEventListener('scroll', handleScroll);

        // Initial check
        updateFloatingTOC();
      }

      // Initialize on load
      document.addEventListener('DOMContentLoaded', function() {
        // Set up sidebar links
        var links = document.querySelectorAll('.sidebar a');
        links.forEach(function(link) {
          link.addEventListener('click', function(e) {
            e.preventDefault();
            var pageId = this.getAttribute('data-page');
            if (pageId) {
              showPage(pageId);
              // Initialize floating TOC for the new page
              setTimeout(function() {
                var pageElement = document.getElementById('page-' + pageId);
                if (pageElement) {
                  initFloatingTOC(pageElement);
                }
              }, 100);
            }
          });
        });

        // Function to handle hash navigation
        function handleHashNavigation() {
          var hash = window.location.hash.substring(1);
          if (!hash) {
            // No hash, show first page by default
            var firstLink = document.querySelector('.sidebar a[data-page]');
            if (firstLink) {
              var firstPageId = firstLink.getAttribute('data-page');
              showPage(firstPageId);
              setTimeout(function() {
                var firstPageElement = document.getElementById('page-' + firstPageId);
                if (firstPageElement) {
                  initFloatingTOC(firstPageElement);
                }
              }, 100);
            }
            return;
          }

          // Check if hash is a page ID
          var pageElement = document.getElementById('page-' + hash);
          if (pageElement) {
            // Hash is a page ID, show that page
            showPage(hash);
            setTimeout(function() {
              initFloatingTOC(pageElement);
            }, 100);
            return;
          }

          // Hash might be a section ID, find which page contains it
          var targetElement = document.getElementById(hash);
          if (targetElement) {
            // Find the page that contains this element
            var containingPage = targetElement.closest('.page');
            if (containingPage) {
              var pageId = containingPage.id.replace('page-', '');
              showPage(pageId);
              setTimeout(function() {
                initFloatingTOC(containingPage);
                // Scroll to the target element
                function getAbsoluteTop(element) {
                  var top = 0;
                  while (element) {
                    top += element.offsetTop;
                    element = element.offsetParent;
                  }
                  return top;
                }
                var absoluteTop = getAbsoluteTop(targetElement);
                window.scrollTo({
                  top: Math.max(0, absoluteTop - 20),
                  behavior: 'smooth'
                });
              }, 200);
              return;
            }
          }

          // Hash doesn't match anything, show first page
          var firstLink = document.querySelector('.sidebar a[data-page]');
          if (firstLink) {
            var firstPageId = firstLink.getAttribute('data-page');
            showPage(firstPageId);
            setTimeout(function() {
              var firstPageElement = document.getElementById('page-' + firstPageId);
              if (firstPageElement) {
                initFloatingTOC(firstPageElement);
              }
            }, 100);
          }
        }

        // Handle hash on load
        handleHashNavigation();

        // Also handle hash changes
        window.addEventListener('hashchange', function() {
          handleHashNavigation();
        });

        // Initialize floating TOC for all pages
        var allPages = document.querySelectorAll('.page');
        allPages.forEach(function(page) {
          initFloatingTOC(page);
        });
      });
    </script>
  JS
end

def highlight_code_blocks(html_content)
  return html_content unless ROUGE_AVAILABLE

  code_block_count = 0
  highlighted_count = 0
  error_count = 0

  # Find all code blocks - handle various formats
  # Apex outputs: <pre lang="language"><code>...</code></pre>
  # Or: <pre><code class="language-xxx">...</code></pre>
  # Match <pre> tag (with optional lang attribute), then <code> tag (with optional class), then content, then closing tags
  result = html_content.gsub(/<pre(\s+lang=["']([^"']+)["'])?[^>]*>\s*<code(\s+class=["'](?:language-)?([^"'\s]+)["'])?[^>]*>([\s\S]*?)<\/code>\s*<\/pre>/i) do |match|
    code_block_count += 1
    lang = $2 || $4  # Get lang from <pre lang="..."> ($2) or class from <code class="..."> ($4)
    code = $5

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

  if code_block_count > 0
    puts "  Highlighted #{highlighted_count}/#{code_block_count} code blocks"
    puts "  Errors: #{error_count}" if error_count > 0
  end

  result
end

def extract_headers(html_content)
  headers = []
  html_content.scan(/<h([1-6])[^>]*id=["']([^"']+)["'][^>]*>([\s\S]*?)<\/h[1-6]>/i) do |level, id, text|
    text = text.gsub(/<[^>]+>/, '').gsub(/\s+/, ' ').strip
    headers << { level: level.to_i, id: id, text: text } unless text.empty?
  end
  headers
end

def generate_page_toc(headers)
  return '' if headers.empty?

  toc_html = '<nav class="page-toc"><ul>'
  stack = []

  headers.each_with_index do |header, idx|
    level = header[:level]
    next_header = headers[idx + 1]
    next_level = next_header ? next_header[:level] : 1

    while !stack.empty? && stack.last >= level
      toc_html += '</ul></li>'
      stack.pop
    end

    if next_level > level
      toc_html += "<li><a href=\"##{header[:id]}\">#{header[:text]}</a><ul>"
      stack.push(level)
    else
      toc_html += "<li><a href=\"##{header[:id]}\">#{header[:text]}</a></li>"
    end
  end

  while !stack.empty?
    toc_html += '</ul></li>'
    stack.pop
  end

  toc_html += '</ul></nav>'
  toc_html
end

def fix_links_in_html(html_content, page_map)
  # Fix internal links to use JavaScript navigation
  html_content.gsub(/<a\s+([^>]*\s+)?href=["']([^"']+)["']([^>]*)>/i) do |match|
    attrs_before = $1 || ''
    href = $2
    attrs_after = $3 || ''

    # Skip external links, anchors (starting with #), or files with extensions
    if href =~ /^(https?:\/\/|mailto:|#|.*\.(html|md|pdf|png|jpg|jpeg|gif|svg|webp))/i
      match
    elsif page_map[href]
      # Convert to JavaScript navigation
      page_id = page_map[href]
      "<a #{attrs_before}href=\"##{page_id}\" onclick=\"showPage('#{page_id}'); return false;\"#{attrs_after}>"
    elsif page_map[href.gsub(/\s+/, '-')]
      page_id = page_map[href.gsub(/\s+/, '-')]
      "<a #{attrs_before}href=\"##{page_id}\" onclick=\"showPage('#{page_id}'); return false;\"#{attrs_after}>"
    else
      # Try to find a matching page by case-insensitive comparison
      matching_page = page_map.find { |name, id| name.downcase == href.downcase || name.downcase.gsub(/\s+/, '-') == href.downcase }
      if matching_page
        page_id = matching_page[1]
        "<a #{attrs_before}href=\"##{page_id}\" onclick=\"showPage('#{page_id}'); return false;\"#{attrs_after}>"
      else
        match
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

puts "Generating single HTML file with all wiki pages..."

# Ensure output directory exists
FileUtils.mkdir_p(HTML_DIR)

# Clone wiki
clone_wiki

unless File.exist?(WIKI_DIR)
  puts "Error: Wiki directory not found at #{WIKI_DIR}"
  exit 1
end

unless File.exist?(APEX_BIN)
  puts "Error: Apex binary not found at #{APEX_BIN}"
  puts "Please build Apex first or ensure it's in PATH"
  cleanup_wiki
  exit 1
end

# Parse sidebar to get page order
pages = parse_sidebar
puts "Found #{pages.length} pages to process..."

# Parse footer
footer_html = parse_footer

# Build complete page_map first (before processing any pages)
# This ensures all page names are available when fixing links
page_map = {}
pages.each do |page_info|
  page_id = page_info[:name].downcase.gsub(/\s+/, '-')
  # Map both the original name and common variations
  page_map[page_info[:name]] = page_id
  page_map[page_info[:name].gsub(/\s+/, '-')] = page_id
  page_map[page_info[:name].gsub(/\s+/, '_')] = page_id
  # Also map the file name without extension
  file_basename = File.basename(page_info[:file], '.md')
  page_map[file_basename] = page_id
  page_map[file_basename.gsub(/\s+/, '-')] = page_id
end

# Process each page
page_htmls = []

pages.each do |page_info|
  md_file = File.join(WIKI_DIR, page_info[:file])
  next unless File.exist?(md_file)

  puts "Processing #{page_info[:name]}..."

  # Convert to HTML
  html_content = `#{APEX_BIN} "#{md_file}" --standalone --pretty 2>/dev/null`

  if $?.success? && !html_content.empty?
    # Extract body content (remove html/head/body tags)
    body_match = html_content.match(/<body[^>]*>([\s\S]*)<\/body>/i)
    if body_match
      body_content = body_match[1]

      # Extract title
      title_match = body_content.match(/<h1[^>]*>(.*?)<\/h1>/i)
      title = title_match ? title_match[1].gsub(/<[^>]+>/, '').strip : page_info[:title]

      # Fix links (page_map is now complete with all pages)
      body_content = fix_links_in_html(body_content, page_map)

      # Extract headers for TOC (after fixing links)
      headers = extract_headers(body_content)
      page_toc = generate_page_toc(headers)

      # Fix any remaining links in the TOC itself
      page_toc = fix_links_in_html(page_toc, page_map)

      # Highlight code blocks
      body_content = highlight_code_blocks(body_content)

      # Inject page TOC after h1 (add ID for scroll detection)
      page_toc_with_id = page_toc.sub(/<nav class="page-toc">/, '<nav class="page-toc" id="page-toc-top">')
      if body_content =~ /<h1[^>]*>[\s\S]*?<\/h1>/i
        body_content = body_content.sub(/(<h1[^>]*>[\s\S]*?<\/h1>)/i, "\\1\n#{page_toc_with_id}")
      end

      # Add footer
      body_content += "\n#{footer_html}" if footer_html.length > 0

      page_htmls << {
        id: page_info[:name].downcase.gsub(/\s+/, '-'),
        title: title,
        content: body_content
      }
    end
  end
end

# Generate sidebar HTML
# Add hamburger menu and mobile overlay
hamburger_html = <<~HTML
  <button class="hamburger-menu" id="hamburger-menu" aria-label="Toggle navigation"></button>
  <div class="mobile-menu-overlay" id="mobile-menu-overlay"></div>
HTML

sidebar_html = hamburger_html + '<nav class="sidebar"><ul>'
pages.each do |page_info|
  page_id = page_info[:name].downcase.gsub(/\s+/, '-')
  sidebar_html += "<li><a href=\"##{page_id}\" data-page=\"#{page_id}\">#{page_info[:title]}</a></li>"
end
sidebar_html += '</ul></nav>'

# Generate page containers
pages_html = '<div class="content">'
page_htmls.each do |page|
  pages_html += "<div id=\"page-#{page[:id]}\" class=\"page\">#{page[:content]}</div>"
end
pages_html += '</div>'

# Combine everything
html_output = <<~HTML
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Apex Documentation</title>
  #{generate_css}
</head>
<body>
  #{sidebar_html}
  #{pages_html}
  #{generate_javascript}
</body>
</html>
HTML

# Write output
output_file = File.join(HTML_DIR, 'apex-docs.html')
File.write(output_file, html_output)

puts "\nSingle HTML file generated successfully!"
puts "Output: #{output_file}"
puts "File size: #{(File.size(output_file) / 1024.0 / 1024.0).round(2)} MB"

# Clean up wiki clone
cleanup_wiki
