/**
 * NSString+ApexPlugins.h
 * Objective-C API for Apex plugin catalog and installation.
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/** Metadata for one plugin (catalog or installed). */
@interface ApexPluginInfo : NSObject

@property (nonatomic, copy, readonly) NSString *pluginId;
@property (nonatomic, copy, readonly, nullable) NSString *title;
@property (nonatomic, copy, readonly, nullable) NSString *pluginDescription;
@property (nonatomic, copy, readonly, nullable) NSString *author;
@property (nonatomic, copy, readonly, nullable) NSString *homepage;
@property (nonatomic, copy, readonly, nullable) NSString *repo;

- (instancetype)init NS_UNAVAILABLE;

@end

/** Plugin catalog and install helpers for app UIs. */
@interface ApexPluginCatalog : NSObject

/** Default apex-plugins.json URL. */
@property (class, nonatomic, readonly) NSString *defaultDirectoryURL;

/**
 * Fetch available plugins from the central directory.
 * @param error Set on failure (network, parse, curl missing).
 */
+ (nullable NSArray<ApexPluginInfo *> *)fetchAvailablePlugins:(NSError **)error;

/**
 * Fetch available plugins from a custom directory JSON URL.
 */
+ (nullable NSArray<ApexPluginInfo *> *)fetchAvailablePluginsFromURL:(NSString *)url
                                                               error:(NSError **)error;

/**
 * List installed plugins (project + user-global, same order as runtime loading).
 * @param baseDirectory Optional document base for .apex/plugins lookup.
 */
+ (NSArray<ApexPluginInfo *> *)installedPluginsWithBaseDirectory:(NSString * _Nullable)baseDirectory;

/**
 * Install a plugin by directory id into ~/.config/apex/plugins.
 */
+ (BOOL)installPluginWithId:(NSString *)pluginId error:(NSError **)error;

/**
 * Install from a Git URL or GitHub shorthand (user/repo). Requires explicit opt-in.
 */
+ (BOOL)installPluginFromRepository:(NSString *)idOrRepo
              allowUntrustedSource:(BOOL)allowUntrusted
                             error:(NSError **)error;

/** Remove a plugin from the user-global plugins directory. */
+ (BOOL)uninstallPluginWithId:(NSString *)pluginId error:(NSError **)error;

/** User-global plugins directory path. */
+ (nullable NSString *)globalPluginsDirectory;

@end

FOUNDATION_EXPORT NSErrorDomain const ApexPluginErrorDomain;

typedef NS_ERROR_ENUM(ApexPluginErrorDomain, ApexPluginError) {
    ApexPluginErrorFetchFailed = 1,
    ApexPluginErrorInstallFailed = 2,
    ApexPluginErrorUninstallFailed = 3,
};

NS_ASSUME_NONNULL_END
