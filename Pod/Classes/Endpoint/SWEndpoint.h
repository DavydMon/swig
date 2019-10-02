







#import <Foundation/Foundation.h>
#import "SWAccount.h"
#import <pjsua.h>
#import "SWRingtone.h"
#import <pjnath.h>
#import "SWMessageSenderProtocol.h"


 
struct Sync {
    uint64_t lastSmidRX;
    uint64_t lastSmidTX;
    uint64_t lastReport;
    uint64_t lastViev;
};

struct Settings {
    __unsafe_unretained NSString *contactServer;
    __unsafe_unretained NSString *pushServer;
    __unsafe_unretained NSString *fileServer;
    __unsafe_unretained NSString *syncServer;
    BOOL homeAbonent;
};


typedef NS_ENUM(NSUInteger, SWMessageStatus) {
    SWMessageStatusSending = 0,
    SWMessageStatusSended = 1,
    SWMessageStatusDelivered = 2,
    SWMessageStatusNotDelivered = 3,
    SWMessageStatusRead = 4
};


typedef void (^SWMessageReceivedBlock)(SWAccount *account, NSString *from, NSString *to, NSString *message, NSUInteger messageID, NSInteger groupID, NSDate *date, SWFileType fileType, NSString *fileHash, NSString *fileServer, BOOL sync, BOOL lastMessageInPack, int status);
typedef void (^SWMessageDeletedBlock)(SWAccount *account, NSUInteger messageID);
typedef void (^SWChatDeletedBlock)(SWAccount *account, NSString* partner, NSUInteger groupID);
typedef void (^SWNeedConfirmBlock)(SWAccount *account, NSUInteger status, NSDictionary *headers);
typedef void (^SWConfirmationBlock)(SWAccount *account, NSError *error);
typedef void (^SWMessageStatusBlock) (SWAccount *account, NSUInteger messageID, SWMessageStatus status, NSDate *date, BOOL sync, BOOL lastMessageInPack);
typedef void (^SWMessageStatusBlockForAbonent) (SWAccount *account, NSUInteger messageID, SWMessageStatus status, NSDate *date, BOOL sync, BOOL lastMessageInPack, NSString *abonent);
typedef void (^SWAbonentStatusBlock) (SWAccount *account, NSString *abonent, SWPresenseState loginStatus, NSDate *lastOnline);
typedef void (^SWGroupMembersUpdatedBlock) (SWAccount *account, NSString *abonent, NSString *admin, NSInteger groupID, BOOL abonentAdded);

typedef struct Sync (^SWGetCounterBlock) (SWAccount *account);
typedef void (^SWSettingsUpdatedBlock) (struct Settings settings);
typedef void (^SWSyncDoneBlock) (SWAccount *account);
typedef void (^SWGroupCreatedBlock) (SWAccount *account, NSInteger groupID, NSString *groupName);
typedef void (^SWTypingBlock) (SWAccount *account, NSString *abonent, NSInteger groupID, BOOL typing);
typedef BOOL (^SWShouldResumeBlock) (SWAccount *account);
typedef void (^SWUnauthorizedBlock) (SWAccount *account);
typedef void (^SWCallVideoFormatChangeBlock)(SWAccount *account, SWCall *call);

typedef void (^SWErrorBlock) (NSUInteger status);
typedef void (^SWErrorObjBlock) (NSError *error);




@class SWEndpointConfiguration, SWRingtoneDescription, SWAccount, SWCall, SWThreadManager, SWIntentManager;

@interface SWEndpoint : NSObject

@property (nonatomic, strong, readonly) SWEndpointConfiguration *endpointConfiguration;


@property (nonatomic, strong) SWAccount *account;
@property (nonatomic, strong) SWRingtone *ringtone;
@property (nonatomic, readonly) BOOL areOtherCalls;

@property (nonatomic, strong) SWThreadManager *threadFactory;
@property (nonatomic, readonly) id<SWMessageSenderProtocol> messageSender;
@property (strong, readonly) SWIntentManager *intentManager;

@property (nonatomic, copy) SWNeedConfirmBlock needConfirmBlock;
@property (nonatomic, copy) SWErrorBlock registerErrorBlock;
@property (nonatomic, copy) SWCallVideoFormatChangeBlock callVideoFormatChangeBlock;
@property (readonly) pjmedia_sdp_session *localSdp;
@property (readonly) pjmedia_sdp_session *remoteSdp;

@property (nonatomic) NSMutableDictionary<NSString *, NSString *> *replacedCallIds;
@property (nonatomic) NSMutableDictionary<NSString *, NSString *> *reverseCallIds;
@property (nonatomic) NSMutableDictionary<NSString *, SWErrorObjBlock> *referAnswerHandlers;

@property (atomic, assign) NSInteger endpointIteration; 

@property (atomic, assign) BOOL isWakingUp;
@property (atomic, assign) BOOL isMediaIpv6Enabled;

+(instancetype)sharedEndpoint;

- (pj_pool_t *) pjPool;

-(void)configure:(SWEndpointConfiguration *)configuration completionHandler:(void(^)(NSError *error))handler; 
-(BOOL)hasTCPConfiguration;
-(void)start:(void(^)(NSError *error))handler;
-(void)reset:(void(^)(NSError *error))handler; 
-(void)restart:(void(^)(NSError *error))handler;
-(void)setShouldResumeBlock:(SWShouldResumeBlock)handler;

-(BOOL)hasActiveAccount;
-(void)addAccount:(SWAccount *)account;
-(void)removeAccount:(SWAccount *)account;
-(SWAccount *)lookupAccount:(NSInteger)accountId;
-(SWAccount *)firstAccount;
-(NSInteger)firstAccountId;

-(void)setAccountStateChangeBlock:(void(^)(SWAccount *account))accountStateChangeBlock forObserver: (id) observer;
-(void)removeAccountStateChangeBlockForObserver: (id) observer;
-(void)setIncomingCallBlock:(void(^)(SWAccount *account, SWCall *call))incomingCallBlock;
-(void)setCallStateChangeBlock:(void(^)(SWAccount *account, SWCall *call, pjsip_status_code statusCode))callStateChangeBlock;
- (void) runCallStateChangeBlockForCall: (SWCall *)call setCode: (pjsip_status_code) statusCode;
-(void)setCallMediaStateChangeBlock:(void(^)(SWAccount *account, SWCall *call))callMediaStateChangeBlock;
-(void)setCallVideoFormatChangeBlock:(void(^)(SWAccount *account, SWCall *call))callVideoFormatChangeBlock;
-(void)setSyncDoneBlock:(void(^)(SWAccount *account))syncDoneBlock;
-(void)setGroupCreatedBlock:(void(^)(SWAccount *account, NSInteger groupID, NSString *groupName))groupCreatedBlock;
- (void) onReferFrom: (NSString *) abonent server: (NSString *)voiceUrl forAccount: (int)acc_id ipV6: (BOOL) ipv6Enabled callId: (NSString *)callId cSeq: (int)cSeq handler: (SWErrorObjBlock)handler;


- (void) setMessageReceivedBlock: (SWMessageReceivedBlock) messageReceivedBlock;
- (void) setMessageDeletedBlock: (SWMessageDeletedBlock) messageDeletedBlock;
- (void) setMessageStatusBlock: (SWMessageStatusBlock) messageStatusBlock;
- (void) setMessageStatusBlockForAbonent: (SWMessageStatusBlockForAbonent) messageStatusBlockForAbonent;
- (void) setAbonentStatusBlock: (SWAbonentStatusBlock) abonentStatusBlock;
- (void) setGroupMembersUpdatedBlock: (SWGroupMembersUpdatedBlock) groupMembersUpdatedBlock;
- (void) setTypingBlock: (SWTypingBlock) typingBlock;
- (void) setChatDeletedBlock: (SWChatDeletedBlock) chatDeletedBlock;

- (void) registerSipThread: (NSThread *) thread;



- (void) setConfirmationBlock: (SWConfirmationBlock) confirmationBlock;

- (void) setUnauthorizedBlock: (SWUnauthorizedBlock) unauthorizedBlock;

- (void) setOtherErrorBlock: (SWErrorBlock) otherErrorBlock;



- (void) setGetCountersBlock: (SWGetCounterBlock) getCountersBlock;
- (void) setSettingsUpdatedBlock: (SWSettingsUpdatedBlock) settingsUpdatedBlock;






- (pj_bool_t) rxRequestPackageProcessing: (pjsip_rx_data *)data;
- (pj_bool_t) rxResponsePackageProcessing: (pjsip_rx_data *)data;
- (pj_bool_t) txResponsePackageProcessing: (pjsip_tx_data *)tdata;
- (pj_bool_t) txRequestPackageProcessing: (pjsip_tx_data *)tdata;

- (void) startStandartRingtone;
- (SWRingtone *) getRingtoneForReason: (NSInteger) reason;
- (SWRingtone *) getRingtoneForReason: (NSInteger) reason andCall: (SWCall *) call;
- (SWRingtone *) getRingtoneForDescription: (SWRingtoneDescription *) description;

+ (NSString *) getHeaderByName: (NSString *) hname forMessage: (pjsip_msg *) msg;
+ (pjsip_contact_hdr *) rightFindHeaderInMessage: (pjsip_msg)msg
                                         forType: (pjsip_hdr_e) type;
+ (pjsip_contact_hdr *) rightFindHeaderInMessage: (pjsip_msg)msg
                                         forName: (const pj_str_t *) name;

@end
