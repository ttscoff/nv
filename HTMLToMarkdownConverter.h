#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface HTMLToMarkdownConverter : NSObject

- (NSString *)markdownFromHTML:(NSString *)html;

@end

NS_ASSUME_NONNULL_END