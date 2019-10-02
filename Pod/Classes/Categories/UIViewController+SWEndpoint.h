







#import <UIKit/UIKit.h>

@class SWAccount;
@interface UIViewController (SWEndpoint)

- (BOOL)sw_shouldObserveAccountStateChanges;
- (void)sw_accountStateChanged:(SWAccount*)account;

@end
