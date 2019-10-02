







#import "SWCall.h"
#import "SWMutableContact.h"

@interface SWMutableCall : SWCall

@property (nonatomic, strong) SWMutableContact *contact;
@property (nonatomic) BOOL inbound;
@property (nonatomic) BOOL missed;
@property (nonatomic) NSDate *date;
@property (nonatomic) NSTimeInterval duration; 

@end
