







#import <Foundation/Foundation.h>
#include "pjsip/sip_types.h"

typedef NS_ENUM(NSUInteger, SWTransportType) {
    SWTransportTypeUDP = PJSIP_TRANSPORT_UDP,
    SWTransportTypeTCP = PJSIP_TRANSPORT_TCP,
    SWTransportTypeUDP6 = PJSIP_TRANSPORT_UDP6,
    SWTransportTypeTCP6 = PJSIP_TRANSPORT_TCP6,
    SWTransportTypeTLS = PJSIP_TRANSPORT_TLS,
    SWTransportTypeTLS6 = PJSIP_TRANSPORT_TLS6
};

@interface SWTransportConfiguration : NSObject

@property (nonatomic) SWTransportType transportType;
@property (nonatomic) NSUInteger port; 
@property (nonatomic) NSUInteger portRange; 




+(instancetype)configurationWithTransportType:(SWTransportType)transportType;

@end
