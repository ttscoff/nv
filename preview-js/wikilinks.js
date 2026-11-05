/* Turn [[note title]] text into nvalt://find/ links. The editor uses the same
   scheme, and the preview opens clicked links with NSWorkspace. Code, math,
   diagrams, and existing links are left alone. */
(function () {
	var skipTags = { A: 1, CODE: 1, PRE: 1, SCRIPT: 1, STYLE: 1, TEXTAREA: 1 };
	var pattern = /\[\[([^\]\n]+?)\]\]/g;

	function skipped(node) {
		var el = node.parentNode;
		while (el && el.nodeType === 1) {
			if (skipTags[el.tagName])
				return true;
			var cls = el.className;
			if (typeof cls === "string" && /(^|\s)(math|mermaid)(\s|$)/.test(cls))
				return true;
			el = el.parentNode;
		}
		return false;
	}

	function linkable(title) {
		if (!title || !title.length)
			return false;
		var first = title.charAt(0);
		var last = title.charAt(title.length - 1);
		if (/\s/.test(first) || /\s/.test(last) || first === "[" || last === "[")
			return false;
		return true;
	}

	function linkWikiText(root) {
		if (!root)
			return;
		var walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT, null, false);
		var nodes = [];
		var node;
		while ((node = walker.nextNode())) {
			if (node.nodeValue.indexOf("[[") !== -1 && !skipped(node))
				nodes.push(node);
		}
		var i;
		for (i = 0; i < nodes.length; i++) {
			var text = nodes[i].nodeValue;
			pattern.lastIndex = 0;
			var frag = document.createDocumentFragment();
			var last = 0;
			var match;
			var linked = false;
			while ((match = pattern.exec(text))) {
				var title = match[1];
				if (!linkable(title))
					continue;
				linked = true;
				if (match.index > last)
					frag.appendChild(document.createTextNode(text.slice(last, match.index)));
				var anchor = document.createElement("a");
				anchor.className = "nv-wikilink";
				anchor.setAttribute("href", "nvalt://find/" + encodeURIComponent(title));
				anchor.appendChild(document.createTextNode(match[0]));
				frag.appendChild(anchor);
				last = match.index + match[0].length;
			}
			if (!linked)
				continue;
			if (last < text.length)
				frag.appendChild(document.createTextNode(text.slice(last)));
			nodes[i].parentNode.replaceChild(frag, nodes[i]);
		}
	}

	function run() {
		linkWikiText(document.body);
	}

	if (document.body)
		run();
	else
		document.addEventListener("DOMContentLoaded", run, false);
})();
