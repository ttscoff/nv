import Foundation
import Apex

@objc(NVApexMarkdown)
public final class NVApexMarkdown: NSObject {
    @objc(htmlFromMarkdown:)
    public static func html(from markdown: String) -> String {
        markdown.apexHTML(mode: .unified)
    }
}
