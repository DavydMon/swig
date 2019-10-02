







#import <Foundation/Foundation.h>
#import "pjsua.h"

@interface NSString (PJString)

+(NSString *)stringWithPJString:(pj_str_t)PJString;
-(pj_str_t)pjString;

@end
