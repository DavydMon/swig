







#import <Foundation/Foundation.h>
#import "pjsua.h"

@protocol SWCallProtocol <NSObject>

-(void)callStateChanged;
-(void)callStateChanged: (pjsua_call_info) callInfo withReason: (NSInteger) reason;
-(void)callStateChangedWithReason: (NSInteger) reason;
-(void)mediaStateChanged;

@end
