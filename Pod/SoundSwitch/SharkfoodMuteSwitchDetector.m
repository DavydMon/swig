






#import "SharkfoodMuteSwitchDetector.h"
#import <AudioToolbox/AudioToolbox.h>
#import <UIKit/UIKit.h>


void SharkfoodSoundMuteNotificationCompletionProc(SystemSoundID  ssID,void* clientData);

@interface SharkfoodMuteSwitchDetector()

@property (nonatomic,assign) NSTimeInterval interval;

@property (nonatomic,assign) SystemSoundID soundId;

@property (nonatomic,assign) BOOL forceEmit;

-(void)complete;

-(void)loopCheck;

-(void)didEnterBackground;

-(void)willReturnToForeground;

-(void)scheduleCall;

@property (nonatomic,assign) BOOL isPaused;

@property (nonatomic,assign) BOOL isPlaying;

@end



void SharkfoodSoundMuteNotificationCompletionProc(SystemSoundID  ssID,void* clientData){
    SharkfoodMuteSwitchDetector* detecotr = (__bridge SharkfoodMuteSwitchDetector*)clientData;
    [detecotr complete];
}


@implementation SharkfoodMuteSwitchDetector

-(id)init{
    self = [super init];
    if (self){
        NSURL* url = [[NSBundle mainBundle] URLForResource:@"mute" withExtension:@"caf"];
        if (AudioServicesCreateSystemSoundID((__bridge CFURLRef)url, &_soundId) == kAudioServicesNoError){
            AudioServicesAddSystemSoundCompletion(self.soundId, CFRunLoopGetMain(), kCFRunLoopDefaultMode, SharkfoodSoundMuteNotificationCompletionProc,(__bridge void *)(self));
            UInt32 yes = 1;
            AudioServicesSetProperty(kAudioServicesPropertyIsUISound, sizeof(_soundId),&_soundId,sizeof(yes), &yes);
            [self performSelector:@selector(loopCheck) withObject:nil afterDelay:1];
            self.forceEmit = YES;
        } else {
            self.soundId = -1;
        }
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didEnterBackground) name:UIApplicationDidEnterBackgroundNotification object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(willReturnToForeground) name:UIApplicationWillEnterForegroundNotification object:nil];
    }
    return self;
}

-(void)didEnterBackground{
    self.isPaused = YES;
}
-(void)willReturnToForeground{
    self.isPaused = NO;
    if (!self.isPlaying){
        [self scheduleCall];
    }
}

-(void)setSilentNotify:(SharkfoodMuteSwitchDetectorBlock)silentNotify{
    _silentNotify = [silentNotify copy];
    self.forceEmit = YES;
}

+(SharkfoodMuteSwitchDetector*)shared{
    static SharkfoodMuteSwitchDetector* sShared = nil;
    if (!sShared)
        sShared = [SharkfoodMuteSwitchDetector new];
    return sShared;
}

-(void)scheduleCall{
    [[self class] cancelPreviousPerformRequestsWithTarget:self selector:@selector(loopCheck) object:nil];
    [self performSelector:@selector(loopCheck) withObject:nil afterDelay:1];
}


-(void)complete{
    self.isPlaying = NO;
    NSTimeInterval elapsed = [NSDate timeIntervalSinceReferenceDate] - self.interval;
    BOOL isMute = elapsed < 0.1; 
    if (self.isMute != isMute || self.forceEmit) {
        self.forceEmit = NO;
        _isMute = isMute;
        if (self.silentNotify)
            self.silentNotify(isMute);
    }
    [self scheduleCall];
}

-(void)loopCheck{
    
    if (!self.isPaused){
        self.interval = [NSDate timeIntervalSinceReferenceDate];
        self.isPlaying = YES;
        AudioServicesPlaySystemSound(self.soundId);
    }
}




-(void)dealloc{
    if (self.soundId != -1){
        AudioServicesRemoveSystemSoundCompletion(self.soundId);
        AudioServicesDisposeSystemSoundID(self.soundId);
    }
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

@end
