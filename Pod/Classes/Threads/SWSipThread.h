






#import <Foundation/Foundation.h>

#include <pjsua-lib/pjsua.h>

struct pj_thread_struct {
    pj_thread_desc values;
};

@interface SWSipThread : NSObject

@property (nonatomic, assign) struct pj_thread_struct desc;

- (instancetype)initWithDesc: (pj_thread_desc) desc;

@end
