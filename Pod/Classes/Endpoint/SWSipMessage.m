






#import "SWSipMessage.h"
#import "SWEndpoint.h"
#import "SWThreadManager.h"

@implementation SWSipMessage

- (BOOL)performIntent {
    SWEndpoint *endpoint = [SWEndpoint sharedEndpoint];
    
    NSThread *messageThread = [endpoint.threadFactory getMessageThread];
    
    [[endpoint firstAccount] performSelector:@selector(sendSipMessage:) onThread:messageThread withObject:self waitUntilDone:NO];
    
    return YES;
}

@end
