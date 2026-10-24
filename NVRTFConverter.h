#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface NVRTFConverter : NSObject

+ (NSString *)markdownFromRTFPath:(NSString *)rtfPath error:(NSError **)error;

+ (NSString *)markdownFromAttributedString:(NSAttributedString *)attrStr;

@end

NS_ASSUME_NONNULL_END
