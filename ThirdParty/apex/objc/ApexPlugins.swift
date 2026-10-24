/**
 * ApexPlugins.swift
 * Swift API for plugin catalog and installation.
 */

import Foundation
@_exported import ApexObjC

/// Metadata for one Apex plugin (available or installed).
public struct ApexPluginInfo: Identifiable, Hashable, Sendable {
    public let id: String
    public let title: String?
    public let description: String?
    public let author: String?
    public let homepage: String?
    public let repo: String?

    init(_ objc: ApexObjC.ApexPluginInfo) {
        self.id = objc.pluginId
        self.title = objc.title
        self.description = objc.pluginDescription
        self.author = objc.author
        self.homepage = objc.homepage
        self.repo = objc.repo
    }
}

/// Fetch, list, install, and uninstall Apex plugins from app code.
public enum ApexPluginManager {
    /// Default `apex-plugins.json` URL.
    public static var defaultDirectoryURL: String {
        ApexPluginCatalog.defaultDirectoryURL
    }

    /// User-global plugins directory (`~/.config/apex/plugins` or XDG equivalent).
    public static var globalPluginsDirectory: String? {
        ApexPluginCatalog.globalPluginsDirectory()
    }

    /// Fetch plugins from the central Apex directory.
    public static func fetchAvailable() throws -> [ApexPluginInfo] {
        let items = try ApexPluginCatalog.fetchAvailablePlugins()
        return items.map(ApexPluginInfo.init)
    }

    /// Fetch plugins from a custom directory JSON URL.
    public static func fetchAvailable(from url: String) throws -> [ApexPluginInfo] {
        let items = try ApexPluginCatalog.fetchAvailablePlugins(fromURL: url)
        return items.map(ApexPluginInfo.init)
    }

    /// List installed plugins (project-scoped + user-global).
    public static func installed(baseDirectory: String? = nil) -> [ApexPluginInfo] {
        ApexPluginCatalog.installedPlugins(withBaseDirectory: baseDirectory)
            .map(ApexPluginInfo.init)
    }

    /// Install a curated plugin by id from the central directory.
    public static func install(id: String) throws {
        try install(idOrRepository: id, allowUntrustedSource: false)
    }

    /// Install by directory id, Git URL, or GitHub `user/repo` shorthand.
    public static func install(idOrRepository: String, allowUntrustedSource: Bool = false) throws {
        if allowUntrustedSource {
            try ApexPluginCatalog.installPlugin(
                fromRepository: idOrRepository,
                allowUntrustedSource: true
            )
        } else {
            try ApexPluginCatalog.installPlugin(withId: idOrRepository)
        }
    }

    /// Remove a plugin from the user-global plugins directory.
    public static func uninstall(id: String) throws {
        try ApexPluginCatalog.uninstallPlugin(withId: id)
    }
}
