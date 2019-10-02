






#import "SWThreadManager.h"

#import "SWEndpoint.h"

#import "DMFileLogger.h"

@interface SWThreadManager () {
    NSThread *_messageThread;
    NSThread *_callManagementThread;
    NSThread *_registrationThread;
}

@end

@implementation SWThreadManager

+ (instancetype) sharedInstance {
    static SWThreadManager *_sharedInstance;
    static dispatch_once_t initOnceToken;
    dispatch_once(&initOnceToken, ^{
        _sharedInstance = [[self alloc] init];
    });
    return _sharedInstance;
}

- (NSThread *) getMessageThread {
#warning experiment работа со звонками и с сообщениями тоже лочат друг друга
    return [self getRegistrationThread];

    NSLog(@"<--threads--> requesting: <MessageThread>");

    if (_messageThread == nil) {
        _messageThread = [[NSThread alloc]  initWithTarget:self selector:@selector(threadKeepAlive:) object:nil];
        _messageThread.name = @"messageThread";
        [_messageThread start];
    }

    [[SWEndpoint sharedEndpoint] registerSipThread:_messageThread];

    return _messageThread;
}

- (NSThread *) getCallManagementThread {

#warning experiment работа со звонками и с аккаунтами лочат друг друга
    return [self getRegistrationThread];

    NSLog(@"<--threads--> requesting: <CallManagementThread>");

    if (_callManagementThread == nil) {
        _callManagementThread = [[NSThread alloc]  initWithTarget:self selector:@selector(threadKeepAlive:) object:nil];
        _callManagementThread.name = @"callManagementThread";
        [_callManagementThread start];
    }

    [[SWEndpoint sharedEndpoint] registerSipThread:_callManagementThread];

    return _callManagementThread;
}

- (NSThread *) getRegistrationThread {

    NSLog(@"<--threads--> requesting: <RegistrationThread>");

    @synchronized (self) {
        if (_registrationThread == nil) {
            _registrationThread = [[NSThread alloc]  initWithTarget:self selector:@selector(threadKeepAlive:) object:nil];
            _registrationThread.name = @"registrationThread";
            [_registrationThread start];
        }
    }

    [[SWEndpoint sharedEndpoint] registerSipThread:_registrationThread];

    return _registrationThread;
}

- (void)threadKeepAlive:(id)data {
    NSRunLoop *runloop = [NSRunLoop currentRunLoop];
    [runloop addPort:[NSMachPort port] forMode:NSDefaultRunLoopMode];

#warning вынести в свойство?
    BOOL isAlive = YES;

    while (isAlive) { 
        [runloop runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]];
    }
}

- (void) runBlockOnRegThread: (void (^)(void)) block wait: (BOOL) wait {
    NSThread *regThread = [self getRegistrationThread];

    [self runBlock:block onThread:regThread wait:wait];
}

- (void) runBlock: (void (^)(void)) block onThread: (NSThread *) thread wait: (BOOL) wait {
    NSLog(@"<--threads--> runBlock to thread: <%@>", thread);
    [self performSelector: @selector(runBlock:) onThread: thread withObject: [block copy] waitUntilDone: wait];
}

- (void) runBlock: (void (^)(void)) block {
    NSLog(@"<--threads--> runBlock started");
    block();
}

@end
