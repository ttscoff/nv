// Content script injected into the active tab on demand from popup actions.
(function () {
  if (window.__nvaltContentLoaded) {
    return;
  }
  window.__nvaltContentLoaded = true;

  var sectionMode = {
    enabled: false,
    highlighted: null,
    selectedBlocks: [],
    originalZoomCaptured: false,
    oldZoom: "",
    currentZoom: 1,
    badge: null
  };

  function showToast(message) {
    var toast = document.createElement("div");
    toast.className = "nvalt-toast";
    toast.textContent = message;
    toast.style.position = "fixed";
    toast.style.left = "50%";
    toast.style.bottom = "16px";
    toast.style.transform = "translateX(-50%)";
    toast.style.zIndex = "2147483647";
    toast.style.padding = "8px 12px";
    toast.style.borderRadius = "8px";
    toast.style.background = "rgba(15, 23, 42, 0.92)";
    toast.style.color = "#ffffff";
    toast.style.font = "12px/1.35 -apple-system, BlinkMacSystemFont, Segoe UI, sans-serif";
    toast.style.boxShadow = "0 8px 24px rgba(0, 0, 0, 0.35)";
    toast.style.pointerEvents = "none";
    document.body.appendChild(toast);

    setTimeout(function () {
      if (toast.parentNode) {
        toast.parentNode.removeChild(toast);
      }
    }, 2200);
  }

  function forceClearMarkedStyles() {
    var marked = document.querySelectorAll(
      "[data-nvalt-selected-outline], [data-nvalt-outline], [data-nvalt-selected='1'], [data-nvalt-orig-outline]"
    );
    Array.prototype.forEach.call(marked, function (el) {
      el.style.outline = el.dataset.nvaltOrigOutline || "";
      el.style.backgroundColor = el.dataset.nvaltOrigBg || "";
      delete el.dataset.nvaltSelectedOutline;
      delete el.dataset.nvaltSelectedBg;
      delete el.dataset.nvaltOutline;
      delete el.dataset.nvaltBg;
      delete el.dataset.nvaltSelected;
      delete el.dataset.nvaltOrigOutline;
      delete el.dataset.nvaltOrigBg;
    });
  }

  function clearExtensionUi() {
    disableSectionMode();
    // Defensive cleanup in case section mode was already off.
    clearHighlight();
    clearSelectedBlocks();
    forceClearMarkedStyles();
    restoreOriginalZoom();
    hideModeBadge();
    document.documentElement.style.cursor = "";
  }

  var pendingSend = { title: "", readability: "0" };

  function rememberSendOptions(request) {
    pendingSend.title = request && typeof request.title === "string" ? request.title : "";
    pendingSend.readability = request && request.readability === "1" ? "1" : "0";
  }

  function navigateToNvALT(kind, payload, message) {
    if (typeof buildMarkdownifyURL !== "function") {
      showToast("Could not build the nvALT link.");
      return;
    }
    var built = buildMarkdownifyURL({
      html: kind === "html" ? payload : "",
      pageURL: kind === "html" ? window.location.href : payload,
      title: pendingSend.title,
      readability: pendingSend.readability === "1"
    });
    if (!built.ok) {
      showToast(built.error);
      return;
    }
    if (message) {
      clearExtensionUi();
      showToast(message);
      setTimeout(function () {
        window.location.href = built.url;
      }, 180);
      return;
    }
    window.location.href = built.url;
  }

  function getSelectionHtml() {
    var selection = window.getSelection ? window.getSelection() : null;
    if (!selection || selection.isCollapsed || selection.rangeCount < 1) {
      return "";
    }
    var container = document.createElement("div");
    for (var i = 0; i < selection.rangeCount; i++) {
      container.appendChild(selection.getRangeAt(i).cloneContents());
    }
    return container.innerHTML;
  }

  function getSelectionState() {
    var selection = window.getSelection ? window.getSelection() : null;
    return !!(
      selection &&
      !selection.isCollapsed &&
      selection.rangeCount > 0 &&
      selection.toString().trim().length > 0
    );
  }

  function findBlockElement(startNode) {
    var el = startNode && startNode.nodeType === 1
      ? startNode
      : startNode && startNode.parentElement;
    if (!el) {
      return null;
    }

    var blockSelector = [
      "article", "aside", "blockquote", "details", "dialog", "div", "dl",
      "dt", "dd", "fieldset", "figcaption", "figure", "footer", "form",
      "h1", "h2", "h3", "h4", "h5", "h6", "header", "hr", "li", "main",
      "nav", "ol", "p", "pre", "section", "table", "tbody", "thead", "tr",
      "td", "th", "ul"
    ].join(",");

    return el.closest(blockSelector) || document.body;
  }

  function clearHighlight() {
    var prev = sectionMode.highlighted;
    if (!prev) {
      return;
    }
    if (isBlockSelected(prev)) {
      prev.style.outline = "2px solid #0ea5e9";
      prev.style.backgroundColor = "rgba(14, 165, 233, 0.18)";
    } else {
      prev.style.outline = prev.dataset.nvaltOrigOutline || "";
      prev.style.backgroundColor = prev.dataset.nvaltOrigBg || "";
    }
    delete prev.dataset.nvaltOutline;
    delete prev.dataset.nvaltBg;
    sectionMode.highlighted = null;
  }

  function isBlockSelected(el) {
    return sectionMode.selectedBlocks.indexOf(el) !== -1;
  }

  function compareDocumentOrder(a, b) {
    if (a === b) {
      return 0;
    }
    var pos = a.compareDocumentPosition(b);
    if (pos & Node.DOCUMENT_POSITION_FOLLOWING) {
      return -1;
    }
    return 1;
  }

  function ensureOriginalStyle(el) {
    if (el.dataset.nvaltOrigOutline === undefined) {
      el.dataset.nvaltOrigOutline = el.style.outline || "";
    }
    if (el.dataset.nvaltOrigBg === undefined) {
      el.dataset.nvaltOrigBg = el.style.backgroundColor || "";
    }
  }

  function addSelectedBlock(el) {
    if (!el || isBlockSelected(el)) {
      return;
    }

    // Enforce no nested multi-selection using both state and DOM markers.
    var marked = document.querySelectorAll("[data-nvalt-selected='1']");
    Array.prototype.forEach.call(marked, function (selected) {
      if (selected === el || selected.contains(el) || el.contains(selected)) {
        removeSelectedBlock(selected);
      }
    });
    sectionMode.selectedBlocks
      .slice()
      .forEach(function (selected) {
        if (selected === el || selected.contains(el) || el.contains(selected)) {
          removeSelectedBlock(selected);
        }
      });

    sectionMode.selectedBlocks.push(el);
    normalizeSelectedBlocks();
    // If the element is currently hover-highlighted, use the saved pre-hover
    // style as the selected baseline so clear/remove restores correctly.
    ensureOriginalStyle(el);
    el.dataset.nvaltSelectedOutline = el.dataset.nvaltOrigOutline || "";
    el.dataset.nvaltSelectedBg = el.dataset.nvaltOrigBg || "";
    el.style.outline = "2px solid #0ea5e9";
    el.style.backgroundColor = "rgba(14, 165, 233, 0.18)";
    resyncSelectedStyles();
  }

  function removeSelectedBlock(el) {
    var idx = sectionMode.selectedBlocks.indexOf(el);
    if (idx !== -1) {
      sectionMode.selectedBlocks.splice(idx, 1);
    }
    el.style.outline = el.dataset.nvaltOrigOutline || "";
    el.style.backgroundColor = el.dataset.nvaltOrigBg || "";
    delete el.dataset.nvaltSelectedOutline;
    delete el.dataset.nvaltSelectedBg;
    delete el.dataset.nvaltOutline;
    delete el.dataset.nvaltBg;
    delete el.dataset.nvaltSelected;
    normalizeSelectedBlocks();
    resyncSelectedStyles();
  }

  function clearSelectedBlocks() {
    while (sectionMode.selectedBlocks.length) {
      removeSelectedBlock(sectionMode.selectedBlocks[0]);
    }
  }

  function normalizeSelectedBlocks() {
    // Keep only top-level selections (no ancestors/descendants together).
    sectionMode.selectedBlocks = sectionMode.selectedBlocks.filter(function (el, idx, arr) {
      if (!el || !el.isConnected) {
        return false;
      }
      for (var i = 0; i < arr.length; i++) {
        if (i === idx) {
          continue;
        }
        var other = arr[i];
        if (!other || !other.isConnected) {
          continue;
        }
        if (other.contains(el)) {
          return false;
        }
      }
      return true;
    });
    sectionMode.selectedBlocks.sort(compareDocumentOrder);
  }

  function resyncSelectedStyles() {
    var marked = document.querySelectorAll(
      "[data-nvalt-selected-outline], [data-nvalt-selected='1'], [data-nvalt-orig-outline]"
    );
    Array.prototype.forEach.call(marked, function (el) {
      if (!isBlockSelected(el)) {
        el.style.outline = el.dataset.nvaltOrigOutline || "";
        el.style.backgroundColor = el.dataset.nvaltOrigBg || "";
        delete el.dataset.nvaltSelectedOutline;
        delete el.dataset.nvaltSelectedBg;
        delete el.dataset.nvaltSelected;
      }
    });

    sectionMode.selectedBlocks.forEach(function (el) {
      ensureOriginalStyle(el);
      if (el.dataset.nvaltSelectedOutline === undefined) {
        el.dataset.nvaltSelectedOutline = el.dataset.nvaltOrigOutline || "";
      }
      if (el.dataset.nvaltSelectedBg === undefined) {
        el.dataset.nvaltSelectedBg = el.dataset.nvaltOrigBg || "";
      }
      el.style.outline = "2px solid #0ea5e9";
      el.style.backgroundColor = "rgba(14, 165, 233, 0.18)";
      el.dataset.nvaltSelected = "1";
    });
  }

  function ensureModeBadge() {
    if (sectionMode.badge) {
      return sectionMode.badge;
    }

    var badge = document.createElement("div");
    badge.textContent = "Section selection mode - click a block element, Esc to cancel";
    badge.style.position = "fixed";
    badge.style.right = "12px";
    badge.style.bottom = "12px";
    badge.style.zIndex = "2147483647";
    badge.style.padding = "8px 10px";
    badge.style.borderRadius = "8px";
    badge.style.background = "rgba(17, 24, 39, 0.9)";
    badge.style.color = "#ffffff";
    badge.style.font = "12px/1.35 -apple-system, BlinkMacSystemFont, Segoe UI, sans-serif";
    badge.style.boxShadow = "0 6px 20px rgba(0, 0, 0, 0.35)";
    badge.style.pointerEvents = "none";
    badge.style.maxWidth = "360px";
    badge.style.textAlign = "left";
    sectionMode.badge = badge;
    return badge;
  }

  function cssEscapeValue(value) {
    if (window.CSS && typeof window.CSS.escape === "function") {
      return window.CSS.escape(value);
    }
    return String(value).replace(/[^a-zA-Z0-9_-]/g, "\\$&");
  }

  function getCssSelectorPath(el) {
    if (!el || el.nodeType !== 1) {
      return "";
    }

    var segments = [];
    var node = el;
    while (node && node.nodeType === 1 && node !== document.body && segments.length < 6) {
      var part = node.tagName.toLowerCase();
      if (node.id) {
        part += "#" + cssEscapeValue(node.id);
        segments.unshift(part);
        break;
      }

      if (node.classList && node.classList.length) {
        var classes = Array.prototype.slice.call(node.classList, 0, 2)
          .map(cssEscapeValue)
          .join(".");
        if (classes) {
          part += "." + classes;
        }
      }

      var parent = node.parentElement;
      if (parent) {
        var index = 1;
        var sib = node;
        while ((sib = sib.previousElementSibling)) {
          if (sib.tagName === node.tagName) {
            index += 1;
          }
        }
        part += ":nth-of-type(" + index + ")";
      }

      segments.unshift(part);
      node = parent;
    }

    if (node === document.body) {
      segments.unshift("body");
    }

    return segments.join(" > ");
  }

  function updateModeBadgeForElement(el) {
    var badge = ensureModeBadge();
    var selector = getCssSelectorPath(el);
    var selectedCount = sectionMode.selectedBlocks.length;
    var selectionNote = selectedCount
      ? "Selected blocks: " +
        selectedCount +
        " (Cmd-click to add/remove)\nPress Return to Send to nvALT\n"
      : "Cmd-click to add multiple blocks; click without Cmd to send\n";
    while (badge.firstChild) {
      badge.removeChild(badge.firstChild);
    }

    var title = document.createElement("div");
    title.style.color = "#ffffff";
    title.textContent = "Section selection mode - Esc to cancel";
    badge.appendChild(title);

    var instruction = document.createElement("div");
    instruction.style.color = "rgba(255,255,255,0.78)";
    instruction.style.marginTop = "2px";
    selectionNote.split("\n").forEach(function (line, idx) {
      if (idx > 0) {
        instruction.appendChild(document.createElement("br"));
      }
      instruction.appendChild(document.createTextNode(line));
    });
    badge.appendChild(instruction);

    var selectorLine = document.createElement("div");
    selectorLine.style.color = "rgba(255,255,255,0.58)";
    selectorLine.style.marginTop = "2px";
    selectorLine.textContent = "Hover: " + (selector || "(none)");
    badge.appendChild(selectorLine);
  }

  function showModeBadge() {
    var badge = ensureModeBadge();
    updateModeBadgeForElement(sectionMode.highlighted);
    if (!document.body.contains(badge)) {
      document.body.appendChild(badge);
    }
  }

  function hideModeBadge() {
    if (sectionMode.badge && sectionMode.badge.parentNode) {
      sectionMode.badge.parentNode.removeChild(sectionMode.badge);
    }
  }

  function applyHighlight(el) {
    if (!el) {
      return;
    }
    if (el === sectionMode.highlighted) {
      // Selection count can change without hover target changing.
      updateModeBadgeForElement(el);
      return;
    }
    clearHighlight();
    if (isBlockSelected(el)) {
      el.style.outline = "2px solid #0284c7";
      el.style.backgroundColor = "rgba(14, 165, 233, 0.24)";
    } else {
      ensureOriginalStyle(el);
      el.dataset.nvaltOutline = el.dataset.nvaltOrigOutline || "";
      el.dataset.nvaltBg = el.dataset.nvaltOrigBg || "";
      el.style.outline = "2px solid #3b82f6";
      el.style.backgroundColor = "rgba(59, 130, 246, 0.15)";
    }
    sectionMode.highlighted = el;
    updateModeBadgeForElement(el);
  }

  function onSectionMouseMove(event) {
    if (!sectionMode.enabled) {
      return;
    }
    applyHighlight(findBlockElement(event.target));
  }

  function onSectionClick(event) {
    if (!sectionMode.enabled) {
      return;
    }
    var anchor = event.target.closest ? event.target.closest("a[href]") : null;

    if (anchor && event.altKey) {
      window.location.href = anchor.href;
      return;
    }

    var block = findBlockElement(event.target);
    if (!block) {
      return;
    }

    event.preventDefault();
    event.stopPropagation();

    if (event.metaKey) {
      if (isBlockSelected(block)) {
        removeSelectedBlock(block);
      } else {
        addSelectedBlock(block);
      }
      applyHighlight(block);
      updateModeBadgeForElement(block);
      return;
    }

    var blocksToSend = sectionMode.selectedBlocks.slice();
    if (!isBlockSelected(block)) {
      blocksToSend.push(block);
      blocksToSend.sort(compareDocumentOrder);
    }

    var html = blocksToSend
      .map(function (el) {
        return el.outerHTML || "";
      })
      .join("");

    if (html) {
      disableSectionMode();
      navigateToNvALT(
        "html",
        html,
        "Sending " + blocksToSend.length + " block" + (blocksToSend.length === 1 ? "" : "s") + " to nvALT"
      );
    }
  }

  function onSectionKeyDown(event) {
    if (event.key === "Escape") {
      disableSectionMode();
    }
    if (event.key === "Enter" && sectionMode.selectedBlocks.length > 0) {
      event.preventDefault();
      event.stopPropagation();
      var blocks = sectionMode.selectedBlocks.slice().sort(compareDocumentOrder);
      var html = blocks.map(function (el) {
        return el.outerHTML || "";
      }).join("");
      var count = blocks.length;
      disableSectionMode();
      navigateToNvALT(
        "html",
        html,
        "Sending " + count + " block" + (count === 1 ? "" : "s") + " to nvALT"
      );
    }
  }

  function enableSectionMode() {
    if (sectionMode.enabled) {
      return;
    }
    sectionMode.enabled = true;
    document.addEventListener("mousemove", onSectionMouseMove, true);
    document.addEventListener("click", onSectionClick, true);
    document.addEventListener("keydown", onSectionKeyDown, true);
    window.addEventListener("keydown", onSectionKeyDown, true);
    document.documentElement.style.cursor = "crosshair";
    showModeBadge();
  }

  function disableSectionMode() {
    if (!sectionMode.enabled) {
      return;
    }
    sectionMode.enabled = false;
    document.removeEventListener("mousemove", onSectionMouseMove, true);
    document.removeEventListener("click", onSectionClick, true);
    document.removeEventListener("keydown", onSectionKeyDown, true);
    window.removeEventListener("keydown", onSectionKeyDown, true);
    document.documentElement.style.cursor = "";
    clearHighlight();
    clearSelectedBlocks();
    hideModeBadge();
  }

  function updateZoom(delta) {
    captureOriginalZoomIfNeeded();
    var next = Math.max(0.2, Math.min(3, sectionMode.currentZoom + delta));
    sectionMode.currentZoom = Math.round(next * 100) / 100;
    document.documentElement.style.zoom = String(sectionMode.currentZoom);
    return sectionMode.currentZoom;
  }

  function fitVerticalToViewport() {
    captureOriginalZoomIfNeeded();
    var scrollHeight = Math.max(
      document.documentElement.scrollHeight,
      document.body ? document.body.scrollHeight : 0
    );
    if (!scrollHeight) {
      return sectionMode.currentZoom;
    }
    var ratio = window.innerHeight / scrollHeight;
    sectionMode.currentZoom = Math.round(Math.min(1, ratio) * 100) / 100;
    sectionMode.currentZoom = Math.max(0.2, sectionMode.currentZoom);
    document.documentElement.style.zoom = String(sectionMode.currentZoom);
    return sectionMode.currentZoom;
  }

  function captureOriginalZoomIfNeeded() {
    if (sectionMode.originalZoomCaptured) {
      return;
    }
    sectionMode.oldZoom = document.documentElement.style.zoom || "";
    sectionMode.originalZoomCaptured = true;
  }

  function restoreOriginalZoom() {
    if (!sectionMode.originalZoomCaptured) {
      return;
    }
    document.documentElement.style.zoom = sectionMode.oldZoom;
    sectionMode.currentZoom = 1;
    sectionMode.originalZoomCaptured = false;
    sectionMode.oldZoom = "";
  }

  chrome.runtime.onMessage.addListener(function (request, sender, sendResponse) {
    try {
      if (!request || !request.type) {
        sendResponse({ ok: false });
        return;
      }

      if (request.type === "ping") {
        sendResponse({ ok: true });
        return;
      }

      if (request.type === "getPageInfo") {
        var meta = document.querySelector('meta[property="og:site_name"]');
        sendResponse({
          ok: true,
          title: document.title || "",
          url: window.location.href,
          hasSelection: getSelectionState(),
          hostname: window.location.hostname || "",
          ogSiteName: meta && meta.content ? meta.content : ""
        });
        return;
      }

      if (request.type === "markdownifyUrl") {
        rememberSendOptions(request);
        disableSectionMode();
        navigateToNvALT("url", window.location.href, "Sending page URL to nvALT");
        sendResponse({ ok: true });
        return;
      }

      if (request.type === "markdownifySelection") {
        rememberSendOptions(request);
        disableSectionMode();
        var html = getSelectionHtml();
        if (html) {
          navigateToNvALT("html", html, "Sending selected HTML to nvALT");
          sendResponse({ ok: true, usedSelection: true });
        } else {
          sendResponse({ ok: false, error: "Nothing is selected." });
        }
        return;
      }

      if (request.type === "selectSectionMode") {
        rememberSendOptions(request);
        enableSectionMode();
        sendResponse({ ok: true });
        return;
      }

      if (request.type === "zoomIn") {
        sendResponse({ ok: true, zoom: updateZoom(0.1) });
        return;
      }

      if (request.type === "zoomOut") {
        sendResponse({ ok: true, zoom: updateZoom(-0.1) });
        return;
      }

      if (request.type === "fitVertical") {
        sendResponse({ ok: true, zoom: fitVerticalToViewport() });
        return;
      }

      sendResponse({ ok: false });
    } catch (e) {
      sendResponse({ ok: false, error: String(e) });
    }
  });
})();

