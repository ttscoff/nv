import Foundation
import Apex

@objc(NVApexMarkdown)
public final class NVApexMarkdown: NSObject {
    @objc(htmlFromMarkdown:)
    public static func html(from markdown: String) -> String {
        markdown.apexHTML(mode: .unified)
    }

    @objc(htmlFromMarkdownIncludingSourcePositions:)
    public static func htmlIncludingSourcePositions(from markdown: String) -> String {
        var options = ApexOptions()
        options.sourcePositions = true
        return markdown.apexHTML(mode: .unified, options: options)
    }
}
