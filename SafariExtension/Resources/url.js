function buildMarkdownifyURL(options) {
  var html = options && options.html ? String(options.html) : "";
  var pageURL = options && options.pageURL ? String(options.pageURL) : "";
  var title = options && options.title ? String(options.title).trim() : "";
  var readability = options && options.readability ? "1" : "0";
  var params = [];

  if (html) {
    params.push("html=" + encodeURIComponent(html));
    if (pageURL) {
      params.push("url=" + encodeURIComponent(pageURL));
    }
  } else {
    params.push("url=" + encodeURIComponent(pageURL));
  }
  if (title) {
    params.push("title=" + encodeURIComponent(title));
  }
  params.push("readability=" + readability);

  var url = "nvalt://markdownify?" + params.join("&");
  if (url.length > 1000000) {
    return { ok: false, error: "That selection is too large to send." };
  }
  return { ok: true, url: url };
}

if (typeof globalThis !== "undefined") {
  globalThis.buildMarkdownifyURL = buildMarkdownifyURL;
}
if (typeof module !== "undefined" && module.exports) {
  module.exports = { buildMarkdownifyURL: buildMarkdownifyURL };
}
