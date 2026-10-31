import Foundation
import HTML2Text
import Readability

/// Markdown produced from HTML with the same HTML2Text and Readability flags Marked uses.
@objc(NVMarkdownResult)
public final class NVMarkdownResult: NSObject {
    @objc public let markdown: String
    @objc public let title: String?

    @objc public init(markdown: String, title: String?) {
        self.markdown = markdown
        self.title = title
        super.init()
    }
}

@objc(NVMarkdownifier)
public final class NVMarkdownifier: NSObject {
    /// Converts HTML to Markdown. Pass `readability` to extract the article first.
    /// `sourceURL` is the page address used to resolve relative links.
    @objc(resultFromHTML:sourceURL:readability:)
    public static func result(from html: String, sourceURL: String?, readability: Bool) -> NVMarkdownResult {
        let pageURL = sourceURL ?? ""
        let trimmed = html.trimmingCharacters(in: .whitespacesAndNewlines)
        if trimmed.isEmpty {
            return NVMarkdownResult(markdown: "", title: nil)
        }

        var title = documentTitle(in: trimmed)
        var htmlForMarkdown = trimmed

        if readability {
            let parser = Readability(html: trimmed, url: pageURL.isEmpty ? nil : pageURL)
            parser.allSpecialHandling = true
            parser.acceptedAnswerOnly = false
            parser.includeAnswerComments = false
            parser.minimumAnswerUpvotes = 0
            if let articleHTML = articleHTML(from: parser) {
                htmlForMarkdown = articleHTML
                if let articleTitle = articleTitle(from: parser) {
                    title = articleTitle
                }
            }
        }

        let linkBaseURL = originBaseURL(from: pageURL)
        let converter = HTML2Text(baseurl: linkBaseURL)
        converter.links_each_paragraph = true
        converter.inline_links = false
        converter.unicode_snob = true
        converter.escape_snob = true
        converter.body_width = 0

        let converted = converter.main(
            baseurl: linkBaseURL,
            data: htmlForMarkdown,
            bodyWidth: 0,
            escapeAll: true
        )
        return NVMarkdownResult(markdown: cleanup(converted), title: title)
    }

    private static func articleHTML(from parser: Readability) -> String? {
        guard parser.start(), let element = parser.articleContent else {
            return nil
        }
        guard let html = try? element.html() else {
            return nil
        }
        let trimmed = html.trimmingCharacters(in: .whitespacesAndNewlines)
        if trimmed.isEmpty || trimmed.localizedCaseInsensitiveContains("Sorry, Readability was unable to parse") {
            return nil
        }
        return html
    }

    private static func articleTitle(from parser: Readability) -> String? {
        guard let element = parser.articleTitle, let text = try? element.text() else {
            return nil
        }
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        return trimmed.isEmpty ? nil : trimmed
    }

    private static func documentTitle(in html: String) -> String? {
        guard let regex = try? NSRegularExpression(
            pattern: "<title[^>]*>(.*?)</title>",
            options: [.caseInsensitive, .dotMatchesLineSeparators]
        ) else {
            return nil
        }
        let range = NSRange(html.startIndex..., in: html)
        guard let match = regex.firstMatch(in: html, options: [], range: range),
            match.numberOfRanges > 1,
            let titleRange = Range(match.range(at: 1), in: html)
        else {
            return nil
        }
        let collapsed = String(html[titleRange])
            .replacingOccurrences(of: "\\s+", with: " ", options: .regularExpression)
            .trimmingCharacters(in: .whitespacesAndNewlines)
        return collapsed.isEmpty ? nil : collapsed
    }

    /// Scheme and host, so HTML2Text can resolve root-relative links.
    private static func originBaseURL(from pageURL: String) -> String {
        guard let url = URL(string: pageURL),
            let scheme = url.scheme,
            let host = url.host
        else {
            return pageURL
        }
        if let port = url.port,
            (scheme == "http" && port != 80) || (scheme == "https" && port != 443)
        {
            return "\(scheme)://\(host):\(port)"
        }
        return "\(scheme)://\(host)"
    }

    private static func cleanup(_ markdown: String) -> String {
        markdown
            .replacingOccurrences(
                of: #"([*-+] .*?)\n+(?=[*-+] )"#,
                with: "$1\n",
                options: .regularExpression
            )
            .replacingOccurrences(
                of: #"(?m)^(\* \* \*)$"#,
                with: "\n\n$1\n\n",
                options: .regularExpression
            )
            .replacingOccurrences(of: #"(?m)\n{2,}"#, with: "\n\n", options: .regularExpression)
            .replacingOccurrences(of: "__BR__", with: "  ")
            .trimmingCharacters(in: .whitespacesAndNewlines)
    }
}
