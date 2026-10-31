function normalizeSite(value) {
  return String(value || "").toLowerCase().replace(/[^\p{L}\p{N}]+/gu, "");
}

function siteNamesFor(ogSiteName, hostname) {
  var names = [];
  if (ogSiteName && String(ogSiteName).trim()) {
    names.push(String(ogSiteName).trim());
  }
  var host = String(hostname || "").trim().replace(/^www\./i, "");
  if (host) {
    names.push(host);
    var label = host.split(".")[0];
    if (label && label.length >= 4) {
      names.push(label);
    }
  }
  return names;
}

// Split on | – — · • » and on " - " with spaces around the hyphen.
var SEPARATORS = /\s*(?:\||\u2013|\u2014|\u00B7|\u2022|\u00BB)\s*|\s+-\s+/;

function cleanNoteTitle(title, siteNames) {
  var original = String(title || "").replace(/^\s+|\s+$/g, "");
  if (!original) {
    return "";
  }
  var parts = original.split(SEPARATORS).map(function (part) {
    return part.replace(/^\s+|\s+$/g, "");
  }).filter(function (part) {
    return part.length > 0;
  });
  if (parts.length < 2) {
    return original;
  }

  var names = siteNames || [];
  var hosts = names.filter(function (name) {
    return String(name).indexOf(".") !== -1;
  });
  var kept = [];
  var removed = 0;
  parts.forEach(function (part) {
    var normalized = normalizeSite(part);
    var matchesName = names.some(function (name) {
      var site = normalizeSite(name);
      return site.length > 0 && site === normalized;
    });
    var matchesHost = hosts.some(function (host) {
      return part.toLowerCase().indexOf(String(host).toLowerCase()) !== -1;
    });
    if (matchesName || matchesHost) {
      removed += 1;
      return;
    }
    kept.push(part);
  });

  if (removed === 0 || kept.length === 0) {
    return original;
  }
  return kept.join(" - ");
}

if (typeof globalThis !== "undefined") {
  globalThis.cleanNoteTitle = cleanNoteTitle;
  globalThis.siteNamesFor = siteNamesFor;
}
if (typeof module !== "undefined" && module.exports) {
  module.exports = {
    cleanNoteTitle: cleanNoteTitle,
    siteNamesFor: siteNamesFor
  };
}
