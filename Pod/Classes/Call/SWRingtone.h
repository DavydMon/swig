







#import <Foundation/Foundation.h>
#import "SWPlayableProtocol.h"
#import <AVFoundation/AVAudioPlayer.h>

@interface SWRingtone : NSObject <SWPlayableProtocol>

@property (nonatomic, readonly) BOOL isPlaying;
@property (nonatomic) float volume;
@property (nonatomic, assign) BOOL noVibrations;
@property (nonatomic, strong, readonly) NSURL *fileURL;
@property (nonatomic) BOOL isFinite;

-(instancetype)initWithFileAtPath:(NSURL *)path;
-(void) startRingtone;

- (void) setAudioPlayerDelegate: (id<AVAudioPlayerDelegate>) delegate;

@end
