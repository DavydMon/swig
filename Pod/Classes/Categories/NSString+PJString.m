







#import "NSString+PJString.h"

@implementation NSString (PJString)

+ (NSString *)stringWithPJString:(pj_str_t)pjString {
    
    return [[NSString alloc] initWithBytes:pjString.ptr length:(NSUInteger)pjString.slen encoding:NSUTF8StringEncoding];
}

- (pj_str_t)pjString {
    
    return pj_str((char *)[self cStringUsingEncoding:NSUTF8StringEncoding]);
}

@end
