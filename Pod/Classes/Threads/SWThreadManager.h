






#import <Foundation/Foundation.h>

@interface SWThreadManager : NSObject

+ (instancetype) sharedInstance;

- (NSThread *) getMessageThread;
- (NSThread *) getCallManagementThread;
- (NSThread *) getRegistrationThread;

- (void) runBlock: (void (^)(void)) block onThread: (NSThread *) thread wait: (BOOL) wait;
- (void) runBlockOnRegThread: (void (^)(void)) block wait: (BOOL) wait;

@end
