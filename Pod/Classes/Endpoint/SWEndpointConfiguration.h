







#import <Foundation/Foundation.h>

@class SWRingtoneDescription, SWCall;

@interface SWEndpointConfiguration : NSObject


@property (nonatomic) NSUInteger maxCalls; 


@property (nonatomic) NSUInteger logLevel; 
@property (nonatomic) NSUInteger logConsoleLevel; 
@property (nonatomic, strong) NSString *logFilename; 
@property (nonatomic) NSUInteger logFileFlags; 


@property (nonatomic) NSUInteger clockRate; 
@property (nonatomic) NSUInteger sndClockRate; 


@property (nonatomic, strong) NSArray *transportConfigurations; 

@property (nonatomic, readonly) NSMutableDictionary <NSNumber *, SWRingtoneDescription *> *ringtones;
@property (nonatomic, copy, nullable) SWRingtoneDescription* (^getRingtoneBlock)(NSInteger reason, SWCall *call);
@property (nonatomic, assign) BOOL callKitCanHandleAudioSession;
@property (nonatomic, copy, nullable) BOOL (^areOtherCallsBlock)();
@property (nonatomic, copy, nullable) void (^logBlock)(NSString *prefix, const char *file, int lineNumber, const char *funcName, NSString *msg);

+(instancetype)configurationWithTransportConfigurations:(NSArray *)transportConfigurations;

@end
