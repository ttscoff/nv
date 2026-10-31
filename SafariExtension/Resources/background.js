var INJECT_FILES = ["url.js", "content-script.js"];

function withActiveTab(callback) {
  chrome.tabs.query({ active: true, currentWindow: true }, function (tabs) {
    callback(tabs && tabs.length ? tabs[0] : null);
  });
}

function injectFiles(tabId, files, callback) {
  var index = 0;
  function next() {
    if (index >= files.length) {
      callback(true);
      return;
    }
    var file = files[index];
    index += 1;
    function afterInject() {
      if (chrome.runtime.lastError) {
        callback(false);
        return;
      }
      next();
    }
    if (chrome.scripting && chrome.scripting.executeScript) {
      chrome.scripting.executeScript({ target: { tabId: tabId }, files: [file] }, afterInject);
      return;
    }
    if (chrome.tabs && chrome.tabs.executeScript) {
      chrome.tabs.executeScript(tabId, { file: file }, afterInject);
      return;
    }
    callback(false);
  }
  next();
}

function sendTabMessage(tabId, message, callback) {
  chrome.tabs.sendMessage(tabId, message, function (response) {
    if (chrome.runtime.lastError) {
      callback(null);
      return;
    }
    callback(response || null);
  });
}

function ensureContentScript(tabId, callback) {
  sendTabMessage(tabId, { type: "ping" }, function (response) {
    if (response && response.ok) {
      callback(true);
      return;
    }
    injectFiles(tabId, INJECT_FILES, callback);
  });
}

chrome.runtime.onMessage.addListener(function (request, sender, sendResponse) {
  if (!request || request.type !== "popupAction") {
    return;
  }
  withActiveTab(function (tab) {
    if (!tab || typeof tab.id !== "number") {
      sendResponse({ ok: false, error: "No active tab." });
      return;
    }
    ensureContentScript(tab.id, function (ready) {
      if (!ready) {
        sendResponse({ ok: false, error: "This page cannot be scripted." });
        return;
      }
      sendTabMessage(tab.id, {
        type: request.action,
        title: request.title || "",
        readability: request.readability === "1" ? "1" : "0"
      }, function (response) {
        sendResponse(response || { ok: false, error: "No response from the page." });
      });
    });
  });
  return true;
});
