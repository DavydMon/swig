







#import <Foundation/Foundation.h>



@class SWAccount, SWContact;

@interface SWUriFormatter : NSObject

+(NSString *)sipUri:(NSString *)uri;
+(NSString *)sipUri:(NSString *)uri fromAccount:(SWAccount *)account;
+(NSString *)sipUriWithPhone:(NSString *)uri fromAccount:(SWAccount *)account toGSM: (BOOL) toGSM;
+(NSString *)sipUri:(NSString *)uri withDisplayName:(NSString *)displayName;
+(SWContact *)contactFromURI:(NSString *)uri;
+ (NSString *) usernameFromURI: (NSString *) URI;


@end
