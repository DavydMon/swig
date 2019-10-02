






#import <Foundation/Foundation.h>
#import "SWAccount.h"
#import "SWIntentProtocol.h"

@interface SWSipMessage : NSObject<SWIntentProtocol>

@property (strong, nonatomic, nullable) NSString *message;
@property (assign, nonatomic) SWFileType fileType;
@property (strong, nonatomic, nullable) NSString *fileHash;
@property (strong, nonatomic, nullable) NSString *URI;
@property (assign, nonatomic) BOOL isGroup;
@property (assign, nonatomic) BOOL forceOffline;
@property (assign, nonatomic) BOOL isGSM;
@property (nonatomic, copy, nullable) void (^completionHandler)(NSError *error, NSString *SMID, NSString *fileServer, NSDate *date);

@end
