






#import "DMFileLogger.h"

#import "SWAccount.h"
#import "SWEndpoint.h"
#import "SWEndpointConfiguration.h"

@implementation DMFileLogger

static BOOL _EWNoLogging = YES;
static NSDateFormatter *ewLogDateFormat;

static unsigned long long _logFileSize = 1024*1024*100;

void _Log(NSString *prefix, const char *file, int lineNumber, const char *funcName, NSString *format,...) {
    va_list ap;
    va_start (ap, format);

    format = [format stringByAppendingString:@"\n"];
    NSString *msg = [[NSString alloc] initWithFormat:format arguments:ap];
    va_end (ap);

    if ([SWEndpoint sharedEndpoint].endpointConfiguration.logBlock != nil) {
        [SWEndpoint sharedEndpoint].endpointConfiguration.logBlock(prefix, file, lineNumber, funcName, msg);
    }
}

void _OldLog(NSString *prefix, const char *file, int lineNumber, const char *funcName, NSString *msg) {
    if (DMFileLogger.noLogging) {
        return;
    }

    fprintf(stderr,"%s%50s:%3d - %s",[prefix UTF8String], funcName, lineNumber, [msg UTF8String]);

    if(DMFileLogger.loggingToFile) {
        [DMFileLogger append: msg];
    }
}

+(void) append: (NSString *) msg{

    BOOL filenameGenerated = NO;

    if (!DMFileLogger.logFileName) {
        DMFileLogger.logFileName = [DMFileLogger generateFilename];
        filenameGenerated = YES;
    }

    
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths objectAtIndex:0];
    NSString *path = [documentsDirectory stringByAppendingPathComponent:DMFileLogger.logFileName];
    
    if (![[NSFileManager defaultManager] fileExistsAtPath:path]){
        [self createFileAt:path withReason:filenameGenerated ? @"newFileName" : @"fileNotExists"];
    }
    else {
        NSDictionary *attributes = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:NULL];
        unsigned long long fileSize = [attributes fileSize];

        
        if (fileSize > DMFileLogger.logFileSize) {
            [self deleteFileAt:[documentsDirectory stringByAppendingPathComponent:DMFileLogger.logFileNameOld]];

            DMFileLogger.logFileNameOld = DMFileLogger.logFileName;
            DMFileLogger.logFileName = [DMFileLogger generateFilename];
            path = [documentsDirectory stringByAppendingPathComponent:DMFileLogger.logFileName];
            [self createFileAt:path withReason:@"logfileSizeTooMuch"];
        }
    }

    
    [self append_lowlevelMsg:msg toFile:path];
}

+ (void) append_lowlevelMsg: (NSString *) msg toFile: (NSString *) path {
    NSFileHandle *handle = [NSFileHandle fileHandleForWritingAtPath:path];
    [handle truncateFileAtOffset:[handle seekToEndOfFile]];
    [handle writeData:[msg dataUsingEncoding:NSUTF8StringEncoding]];
    [handle closeFile];
}

+ (void) createFileAt: (NSString *) path withReason: (NSString *) reason {
    fprintf(stderr,"Creating file at %s",[path UTF8String]);
    [[NSData data] writeToFile:path atomically:YES];
    [self append_lowlevelMsg:[NSString stringWithFormat:@"<--logfile--> logfile created with Reason:%@", reason] toFile:path];
}

+ (void) deleteFileAt: (NSString *) path {
    fprintf(stderr,"Deleting file at %s",[path UTF8String]);
    NSError *error;
    if ([[NSFileManager defaultManager] isDeletableFileAtPath:path]) {
        BOOL success = [[NSFileManager defaultManager] removeItemAtPath:path error:&error];
        if (!success) {
            
        }
    }
}

+ (NSString *) generateFilename {
    return [NSString stringWithFormat:@"logfile%@.txt", [[NSUUID UUID] UUIDString]];
}

#pragma mark getters/setters

+(NSString *)logFileNameOld {
    return [[NSUserDefaults standardUserDefaults] objectForKey:@"EWLogFileNameOld"];
}

+(NSString *)logFileName {
    return [[NSUserDefaults standardUserDefaults] objectForKey:@"EWLogFileName"];
}

+(unsigned long long)logFileSize {
    NSNumber *size = [[NSUserDefaults standardUserDefaults] objectForKey:@"EWLogFileSize"];

    return [size unsignedLongLongValue];
}

+(BOOL) loggingToFile {
    return [[NSUserDefaults standardUserDefaults] boolForKey:@"EWLoggingToFile"];
}

+(BOOL) noLogging {
    return _EWNoLogging;
}

+ (void)setLogFileName:(NSString *)logFileName {
    [[NSUserDefaults standardUserDefaults] setObject:logFileName forKey:@"EWLogFileName"];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

+(void)setLogFileNameOld:(NSString *)logFileNameOld {
    [[NSUserDefaults standardUserDefaults] setObject:logFileNameOld forKey:@"EWLogFileNameOld"];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

+(void)setLogFileSize:(unsigned long long)logFileSize {
    NSNumber *size = [NSNumber numberWithUnsignedLongLong:logFileSize];
    [[NSUserDefaults standardUserDefaults] setObject:size forKey:@"EWLogFileSize"];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

+(void)setLoggingToFile:(BOOL)loggingToFile {
    [[NSUserDefaults standardUserDefaults] setBool:loggingToFile forKey:@"EWLoggingToFile"];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

+(void)setNoLogging:(BOOL)noLogging {
    _EWNoLogging = noLogging;
}

@end
