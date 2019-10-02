






#import <Foundation/Foundation.h>

@protocol SWIntentProtocol;

@interface SWIntentManager : NSObject

- (void) start;
- (void) addIntent: (id<SWIntentProtocol>) intent;

@end
