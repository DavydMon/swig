







#import "SWTransportConfiguration.h"

#define kSWPort 5060
#define kSWPortRange 0

@implementation SWTransportConfiguration

-(instancetype)init {
    
    self = [super init];
    
    if (!self) {
        return nil;
    }
    
    _port = kSWPort;
    _portRange = kSWPortRange;
    
    return self;
}

+(instancetype)configurationWithTransportType:(SWTransportType)transportType {
    
    SWTransportConfiguration *configuration = [SWTransportConfiguration new];
    configuration.transportType = transportType;
    
    return configuration;
}

@end
