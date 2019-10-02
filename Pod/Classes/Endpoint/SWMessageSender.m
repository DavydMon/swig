






#import "SWMessageSender.h"
#import "SWSipMessage.h"
#import "SWEndpoint.h"
#import "SWIntentManager.h"

@implementation SWMessageSender

-(void)sendMessage:(NSString *)message fileType:(SWFileType) fileType fileHash:(NSString *) fileHash to:(NSString *)URI isGroup:(BOOL) isGroup forceOffline:(BOOL) forceOffline isGSM:(BOOL) isGSM completionHandler:(void(^)(NSError *error, NSString *SMID, NSString *fileServer, NSDate *date))handler {
    
    SWSipMessage *messageparams = [[SWSipMessage alloc] init];
    messageparams.message = message;
    messageparams.fileType = fileType;
    messageparams.fileHash = fileHash;
    messageparams.URI = URI;
    messageparams.isGroup = isGroup;
    messageparams.forceOffline = forceOffline;
    messageparams.isGSM = isGSM;
    messageparams.completionHandler = handler;
    
    SWEndpoint *endpoint = [SWEndpoint sharedEndpoint];
    
    [endpoint.intentManager addIntent:messageparams];
}

-(void)sendMessage:(NSString *)message fileType:(SWFileType) fileType fileHash:(NSString *) fileHash to:(NSString *)URI isGroup:(BOOL) isGroup completionHandler:(void(^)(NSError *error, NSString *SMID, NSString *fileServer, NSDate *date))handler {
    [self sendMessage:message fileType:fileType fileHash:fileHash to:URI isGroup:isGroup forceOffline:NO isGSM:NO completionHandler:handler];
}

@end
