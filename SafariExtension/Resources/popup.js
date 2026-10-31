var titleInput = document.getElementById("note-title");
var readabilityInput = document.getElementById("readability");
var statusNode = document.getElementById("status");
var sendButtons = [
  document.getElementById("markdownify-url"),
  document.getElementById("markdownify-selection"),
  document.getElementById("select-section")
];

function setStatus(message) {
  if (!message) {
    statusNode.hidden = true;
    statusNode.textContent = "";
    return;
  }
  statusNode.hidden = false;
  statusNode.textContent = message;
}

function callAction(action, callback) {
  chrome.runtime.sendMessage({
    type: "popupAction",
    action: action,
    title: titleInput.value || "",
    readability: readabilityInput.checked ? "1" : "0"
  }, function (response) {
    callback(response || { ok: false, error: "No response from the extension." });
  });
}

function closePopupSoon() {
  setTimeout(function () {
    window.close();
  }, 40);
}

function bindAction(buttonId, action, closeAfter) {
  var button = document.getElementById(buttonId);
  button.addEventListener("click", function () {
    if (button.disabled) {
      return;
    }
    callAction(action, function (response) {
      if (!response.ok) {
        setStatus(response.error || "Could not send this page.");
        return;
      }
      if (closeAfter) {
        closePopupSoon();
      }
    });
  });
}

function loadReadability() {
  try {
    chrome.storage.local.get({ readability: false }, function (items) {
      if (chrome.runtime.lastError) {
        readabilityInput.checked = false;
        return;
      }
      readabilityInput.checked = !!items.readability;
    });
  } catch (e) {
    readabilityInput.checked = false;
  }
  readabilityInput.addEventListener("change", function () {
    try {
      chrome.storage.local.set({ readability: readabilityInput.checked }, function () {
        if (chrome.runtime.lastError) {
          readabilityInput.checked = false;
        }
      });
    } catch (e) {
      readabilityInput.checked = false;
    }
  });
}

function loadPage() {
  callAction("getPageInfo", function (response) {
    if (!response.ok) {
      setStatus(response.error || "This page cannot be scripted.");
      sendButtons.forEach(function (button) {
        button.disabled = true;
      });
      return;
    }
    var names = siteNamesFor(response.ogSiteName, response.hostname);
    titleInput.value = cleanNoteTitle(response.title || "", names);
    document.getElementById("markdownify-selection").disabled = !response.hasSelection;
  });
}

bindAction("markdownify-url", "markdownifyUrl", true);
bindAction("markdownify-selection", "markdownifySelection", true);
bindAction("select-section", "selectSectionMode", true);
bindAction("zoom-in", "zoomIn", false);
bindAction("zoom-out", "zoomOut", false);
bindAction("fit-vertical", "fitVertical", false);
loadReadability();
loadPage();
