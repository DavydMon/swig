







#import <Foundation/Foundation.h>
#import <pjsua.h>
#import "SWPlayableProtocol.h"

@interface SWRingback : NSObject <SWPlayableProtocol>

@property (nonatomic, readonly) NSInteger ringbackSlot;
@property (nonatomic, readonly) pjmedia_port *ringbackPort;

@end
