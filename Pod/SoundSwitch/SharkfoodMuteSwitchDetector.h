






#import <Foundation/Foundation.h>


typedef void(^SharkfoodMuteSwitchDetectorBlock)(BOOL silent);

@interface SharkfoodMuteSwitchDetector : NSObject

+(SharkfoodMuteSwitchDetector*)shared;

@property (nonatomic,readonly) BOOL isMute;
@property (nonatomic,copy) SharkfoodMuteSwitchDetectorBlock silentNotify;

@end
