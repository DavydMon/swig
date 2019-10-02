






#import "SWAudioSessionObserver.h"

#import "SWCall.h"
#import "SWEndpoint.h"
#import "SWAccount.h"
#import "DMFileLogger.h"

#import <AVFoundation/AVFoundation.h>


@implementation SWAudioSessionObserver

- (instancetype)init {
    self = [super init];
    if (self) {
        
        NSNotificationCenter *notCenter = [NSNotificationCenter defaultCenter];
        [notCenter addObserver:self selector:@selector(audioSessionRouteDidChangeWithNotification:) name:AVAudioSessionRouteChangeNotification object:nil];
        
        
    }
    return self;
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void) audioSessionRouteDidChangeWithNotification: (NSNotification *) notification {
    
    
    NSDictionary *interuptionDict = notification.userInfo;
    
    NSInteger routeChangeReason = [[interuptionDict valueForKey:AVAudioSessionRouteChangeReasonKey] integerValue];
    
    switch (routeChangeReason) {
        case AVAudioSessionRouteChangeReasonUnknown:
            NSLog(@"<--AudioSession notification--> routeChangeReason : AVAudioSessionRouteChangeReasonUnknown");
            break;
            
        case AVAudioSessionRouteChangeReasonNewDeviceAvailable:
            
            NSLog(@"<--AudioSession notification--> routeChangeReason : AVAudioSessionRouteChangeReasonNewDeviceAvailable");
            break;
            
        case AVAudioSessionRouteChangeReasonOldDeviceUnavailable: {
            
            NSLog(@"<--AudioSession notification--> routeChangeReason : AVAudioSessionRouteChangeReasonOldDeviceUnavailable");
            
            SWCall *call = [[[SWEndpoint sharedEndpoint] firstAccount] firstCall];
            [call updateOverrideSpeaker];
        }
            break;
            
        case AVAudioSessionRouteChangeReasonCategoryChange:
            
            NSLog(@"<--AudioSession notification--> routeChangeReason : AVAudioSessionRouteChangeReasonCategoryChange");
            break;
            
        case AVAudioSessionRouteChangeReasonOverride:
            NSLog(@"<--AudioSession notification--> routeChangeReason : AVAudioSessionRouteChangeReasonOverride");
            break;
            
        case AVAudioSessionRouteChangeReasonWakeFromSleep:
            NSLog(@"<--AudioSession notification--> routeChangeReason : AVAudioSessionRouteChangeReasonWakeFromSleep");
            break;
            
        case AVAudioSessionRouteChangeReasonNoSuitableRouteForCategory:
            NSLog(@"<--AudioSession notification--> routeChangeReason : AVAudioSessionRouteChangeReasonNoSuitableRouteForCategory");
            break;
            
        default:
            break;
    }
}

- (void) audioSessionInterruptionWithNotification: (NSNotification *) notification {
    NSLog(@"<--audiosession interruption--> notification received");
}

- (void) videoErrorNotification: (NSNotification *) notification {
    NSLog(@"<--videoErrorNotification--> Notification:%@",notification);
}

- (void) videoStartNotification: (NSNotification *) notification {
    NSLog(@"<--videoStartNotification--> Notification:%@",notification);
}

- (void) videoDidStopNotification: (NSNotification *) notification {
    NSLog(@"<--videoDidStopNotification--> Notification:%@",notification);
}

- (void) videoWasInterruptedNotification: (NSNotification *) notification {
    if (@available(iOS 9.0, *)) {
        NSLog(@"<--starting--> videoWasInterruptedNotification");
        
        NSLog(@"<--videoWasInterruptedNotification--> key: %d Notification:%@",notification.userInfo[AVCaptureSessionInterruptionReasonKey] == AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableInBackground, notification);
    } else {
        
    }
}

@end
