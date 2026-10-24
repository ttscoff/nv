/**
 * NSString+ApexPlugins.m
 */

#import "NSString+ApexPlugins.h"
#import <apex/plugins.h>

NSErrorDomain const ApexPluginErrorDomain = @"ApexPluginErrorDomain";

@interface ApexPluginInfo ()
@property (nonatomic, copy, readwrite) NSString *pluginId;
@property (nonatomic, copy, readwrite, nullable) NSString *title;
@property (nonatomic, copy, readwrite, nullable) NSString *pluginDescription;
@property (nonatomic, copy, readwrite, nullable) NSString *author;
@property (nonatomic, copy, readwrite, nullable) NSString *homepage;
@property (nonatomic, copy, readwrite, nullable) NSString *repo;
@end

@implementation ApexPluginInfo

+ (instancetype)infoWithCStruct:(const apex_plugin_info *)info {
    if (!info || !info->id) return nil;
    ApexPluginInfo *obj = [[self alloc] initPrivate];
    obj.pluginId = [NSString stringWithUTF8String:info->id];
    if (info->title) obj.title = [NSString stringWithUTF8String:info->title];
    if (info->description) obj.pluginDescription = [NSString stringWithUTF8String:info->description];
    if (info->author) obj.author = [NSString stringWithUTF8String:info->author];
    if (info->homepage) obj.homepage = [NSString stringWithUTF8String:info->homepage];
    if (info->repo) obj.repo = [NSString stringWithUTF8String:info->repo];
    return obj;
}

- (instancetype)initPrivate {
    return [super init];
}

@end

static NSArray<ApexPluginInfo *> *ApexPluginInfosFromCatalog(apex_plugin_catalog *catalog) {
    if (!catalog) return @[];

    NSMutableArray<ApexPluginInfo *> *items = [NSMutableArray arrayWithCapacity:catalog->count];
    for (size_t i = 0; i < catalog->count; i++) {
        ApexPluginInfo *info = [ApexPluginInfo infoWithCStruct:&catalog->items[i]];
        if (info) [items addObject:info];
    }
    return [items copy];
}

static NSError *ApexPluginMakeError(ApexPluginError code, const char *message) {
    NSString *desc = message ? [NSString stringWithUTF8String:message] : @"Unknown plugin error";
    return [NSError errorWithDomain:ApexPluginErrorDomain
                               code:code
                           userInfo:@{NSLocalizedDescriptionKey: desc}];
}

@implementation ApexPluginCatalog

+ (NSString *)defaultDirectoryURL {
    return [NSString stringWithUTF8String:APEX_PLUGIN_DIRECTORY_URL];
}

+ (nullable NSArray<ApexPluginInfo *> *)fetchAvailablePlugins:(NSError **)error {
    return [self fetchAvailablePluginsFromURL:self.defaultDirectoryURL error:error];
}

+ (nullable NSArray<ApexPluginInfo *> *)fetchAvailablePluginsFromURL:(NSString *)url
                                                               error:(NSError **)error {
    if (!url.length) {
        if (error) {
            *error = ApexPluginMakeError(ApexPluginErrorFetchFailed, "URL is required");
        }
        return nil;
    }

    apex_plugin_catalog *catalog = apex_plugin_catalog_fetch_url(url.UTF8String);
    if (!catalog) {
        if (error) {
            *error = ApexPluginMakeError(ApexPluginErrorFetchFailed,
                                         "failed to fetch plugin directory");
        }
        return nil;
    }

    NSArray<ApexPluginInfo *> *items = ApexPluginInfosFromCatalog(catalog);
    apex_plugin_catalog_free(catalog);
    return items;
}

+ (NSArray<ApexPluginInfo *> *)installedPluginsWithBaseDirectory:(NSString *)baseDirectory {
    apex_plugin_discovery_options opts;
    memset(&opts, 0, sizeof(opts));
    opts.include_project = true;
    opts.include_user_global = true;
    if (baseDirectory.length > 0) {
        opts.base_directory = baseDirectory.UTF8String;
    }

    apex_plugin_catalog *catalog = apex_plugins_list_installed(&opts);
    if (!catalog) return @[];

    NSArray<ApexPluginInfo *> *items = ApexPluginInfosFromCatalog(catalog);
    apex_plugin_catalog_free(catalog);
    return items;
}

+ (BOOL)installPluginWithId:(NSString *)pluginId error:(NSError **)error {
    return [self installPluginFromRepository:pluginId allowUntrustedSource:NO error:error];
}

+ (BOOL)installPluginFromRepository:(NSString *)idOrRepo
              allowUntrustedSource:(BOOL)allowUntrusted
                             error:(NSError **)error {
    if (!idOrRepo.length) {
        if (error) {
            *error = ApexPluginMakeError(ApexPluginErrorInstallFailed, "plugin id is required");
        }
        return NO;
    }

    char errbuf[512];
    errbuf[0] = '\0';

    apex_plugin_install_options opts;
    memset(&opts, 0, sizeof(opts));
    opts.allow_untrusted_repo = allowUntrusted;
    opts.run_post_install = true;

    int rc = apex_plugin_install(idOrRepo.UTF8String, &opts, errbuf, sizeof(errbuf));
    if (rc != 0) {
        if (error) {
            *error = ApexPluginMakeError(ApexPluginErrorInstallFailed,
                                         errbuf[0] ? errbuf : "install failed");
        }
        return NO;
    }
    return YES;
}

+ (BOOL)uninstallPluginWithId:(NSString *)pluginId error:(NSError **)error {
    if (!pluginId.length) {
        if (error) {
            *error = ApexPluginMakeError(ApexPluginErrorUninstallFailed, "plugin id is required");
        }
        return NO;
    }

    char errbuf[512];
    errbuf[0] = '\0';
    int rc = apex_plugin_uninstall(pluginId.UTF8String, errbuf, sizeof(errbuf));
    if (rc != 0) {
        if (error) {
            *error = ApexPluginMakeError(ApexPluginErrorUninstallFailed,
                                         errbuf[0] ? errbuf : "uninstall failed");
        }
        return NO;
    }
    return YES;
}

+ (nullable NSString *)globalPluginsDirectory {
    char buf[1024];
    if (apex_plugin_global_directory(buf, sizeof(buf)) != 0) {
        return nil;
    }
    return [NSString stringWithUTF8String:buf];
}

@end
