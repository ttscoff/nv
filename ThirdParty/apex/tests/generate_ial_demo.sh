#!/bin/sh
# Generate ial_demo.html from ial_demo.md

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
APEX_BIN="$SCRIPT_DIR/../build/apex"
MD_FILE="$SCRIPT_DIR/ial_demo.md"
HTML_FILE="$SCRIPT_DIR/ial_demo.html"

# Check if apex binary exists
if [ ! -f "$APEX_BIN" ]; then
    echo "Error: apex binary not found at $APEX_BIN" >&2
    echo "Please build apex first with: make -C build" >&2
    exit 1
fi

# Generate HTML content
HTML_CONTENT=$("$APEX_BIN" "$MD_FILE" 2>&1)
if [ $? -ne 0 ]; then
    echo "Error: Failed to generate HTML from markdown" >&2
    exit 1
fi

# Create the complete HTML file
cat > "$HTML_FILE" << 'EOF'
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>IAL Demo - Attribute Inspector</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
            line-height: 1.6;
            max-width: 900px;
            margin: 0 auto;
            padding: 20px;
            background-color: #f5f5f5;
        }

        .content {
            background: white;
            padding: 30px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }

        /* Style for elements with attributes - add visual indicator */
        [class]:hover,
        [id]:hover {
            outline: 2px solid #4CAF50;
            outline-offset: 2px;
            background-color: rgba(76, 175, 80, 0.1);
            transition: background-color 0.2s;
        }
    </style>
    <script>
        // Add tooltips showing all attributes on hover
        document.addEventListener('DOMContentLoaded', function() {
            // Create tooltip element
            const tooltip = document.createElement('div');
            tooltip.id = 'attribute-tooltip';
            tooltip.style.cssText = 'position: absolute; background: #333; color: white; padding: 8px 12px; border-radius: 4px; font-size: 12px; font-family: "Courier New", monospace; z-index: 10000; pointer-events: none; opacity: 0; transition: opacity 0.2s; box-shadow: 0 2px 8px rgba(0,0,0,0.3); max-width: 400px; word-wrap: break-word; white-space: normal; line-height: 1.4;';
            document.body.appendChild(tooltip);

            // Function to get all attributes as a formatted string
            function getAttributesString(el) {
                const attrs = [];
                if (el.id) attrs.push('id="' + el.id + '"');
                if (el.className) attrs.push('class="' + el.className + '"');
                for (let i = 0; i < el.attributes.length; i++) {
                    const attr = el.attributes[i];
                    if (attr.name !== 'id' && attr.name !== 'class' && !attr.name.startsWith('data-apex-')) {
                        attrs.push(attr.name + '="' + attr.value + '"');
                    }
                }
                return attrs.length > 0 ? attrs.join(' ') : 'No attributes';
            }

            // Add hover handlers to all elements with attributes
            const elementsWithAttrs = document.querySelectorAll('[class], [id]');
            elementsWithAttrs.forEach(el => {
                // Skip the tooltip itself and elements inside code blocks
                if (el.id === 'attribute-tooltip' || el.closest('code') || el.closest('pre')) {
                    return;
                }

                el.addEventListener('mouseenter', function(e) {
                    const attrs = getAttributesString(this);
                    const tagName = this.tagName.toLowerCase();
                    tooltip.innerHTML = '<strong>&lt;' + tagName + '</strong> ' +
                                      (attrs !== 'No attributes' ? attrs : '<em>' + attrs + '</em>') +
                                      '<strong>&gt;</strong>';
                    tooltip.style.opacity = '1';

                    const rect = this.getBoundingClientRect();
                    let left = rect.left + window.scrollX;
                    let top = rect.top + window.scrollY - tooltip.offsetHeight - 8;

                    // Position tooltip
                    tooltip.style.left = left + 'px';
                    tooltip.style.top = top + 'px';

                    // Adjust if tooltip goes off screen
                    setTimeout(() => {
                        const tooltipRect = tooltip.getBoundingClientRect();
                        if (tooltipRect.left < 0) {
                            tooltip.style.left = '10px';
                        }
                        if (tooltipRect.top < 0) {
                            tooltip.style.top = (rect.bottom + window.scrollY + 8) + 'px';
                        }
                        if (tooltipRect.right > window.innerWidth) {
                            tooltip.style.left = (window.innerWidth - tooltipRect.width - 10) + 'px';
                        }
                    }, 0);
                });

                el.addEventListener('mouseleave', function() {
                    tooltip.style.opacity = '0';
                });
            });

            console.log('Attribute inspector loaded. Hover over elements with attributes to see them.');
        });
    </script>
</head>
<body>
    <div class="content">
EOF

# Append the generated HTML content
echo "$HTML_CONTENT" >> "$HTML_FILE"

# Close the HTML
cat >> "$HTML_FILE" << 'EOF'
    </div>
</body>
</html>
EOF

echo "Generated $HTML_FILE"

