#include <utility>

#include "RingBuffer.h"
#include "RCP_Host/RCP_Host.h"
#include "gtest/gtest.h"

// Exposing some internals for testing purposes
extern "C" {
extern RCP_Channel channel;
extern RCP_PromptDataType activePromptType;
extern RCP_LibInitData* callbacks;
extern uint8_t* buffer;
RCP_Error processIU(RCP_DeviceClass devclass, uint32_t timestamp, uint16_t params, const uint8_t* postTS, size_t* inc);
}

// Emtpy stub callbacks for tests that don't need a particular callback
static size_t SEND_STUB(const void*, size_t len) { return len; }
static size_t RCV_STUB(void*, size_t len) { return len; }
static RCP_Error TEST_STUB(RCP_TestData) { return RCP_ERR_SUCCESS; }
static RCP_Error BOOL_STUB(RCP_BoolData) { return RCP_ERR_SUCCESS; }
static RCP_Error SACT_STUB(RCP_SimpleActuatorData) { return RCP_ERR_SUCCESS; }
static RCP_Error PROMPT_STUB(RCP_PromptInputRequest) { return RCP_ERR_SUCCESS; }
static RCP_Error LOG_STUB(RCP_TargetLogData) { return RCP_ERR_SUCCESS; }
static RCP_Error F1_STUB(RCP_1F) { return RCP_ERR_SUCCESS; }
static RCP_Error F2_STUB(RCP_2F) { return RCP_ERR_SUCCESS; }
static RCP_Error F3_STUB(RCP_3F) { return RCP_ERR_SUCCESS; }
static RCP_Error F4_STUB(RCP_4F) { return RCP_ERR_SUCCESS; }

RCP_LibInitData CALLBACK_STUBS = {.sendData = SEND_STUB,
                                  .readData = RCV_STUB,
                                  .processTestUpdate = TEST_STUB,
                                  .processBoolData = BOOL_STUB,
                                  .processSimpleActuatorData = SACT_STUB,
                                  .processPromptInput = PROMPT_STUB,
                                  .processTargetLog = LOG_STUB,
                                  .processOneFloat = F1_STUB,
                                  .processTwoFloat = F2_STUB,
                                  .processThreeFloat = F3_STUB,
                                  .processFourFloat = F4_STUB};

// Equality operators for all the RCP structures
bool operator==(const RCP_TestData& left, const RCP_TestData& right) {
    return std::tie(left.timestamp, left.dataStreaming, left.state, left.isInited, left.heartbeatTime, left.runningTest,
                    left.testProgress) ==
        std::tie(right.timestamp, right.dataStreaming, right.state, right.isInited, right.heartbeatTime,
                 right.runningTest, right.testProgress);
}

bool operator==(const RCP_SimpleActuatorData& left, const RCP_SimpleActuatorData& right) {
    return std::tie(left.timestamp, left.ID, left.state) == std::tie(right.timestamp, right.ID, right.state);
}

bool operator==(const RCP_BoolData& left, const RCP_BoolData& right) {
    return std::tie(left.timestamp, left.ID, left.data) == std::tie(right.timestamp, right.ID, right.data);
}

bool operator==(const RCP_1F& left, const RCP_1F& right) {
    return std::tie(left.devclass, left.timestamp, left.ID, left.data) ==
        std::tie(right.devclass, right.timestamp, right.ID, right.data);
}

bool operator==(const RCP_2F& left, const RCP_2F& right) {
    return std::tie(left.devclass, left.timestamp, left.ID, left.data[0], left.data[1]) ==
        std::tie(right.devclass, right.timestamp, right.ID, right.data[0], right.data[1]);
}

bool operator==(const RCP_3F& left, const RCP_3F& right) {
    return std::tie(left.devclass, left.timestamp, left.ID, left.data[0], left.data[1], left.data[2]) ==
        std::tie(right.devclass, right.timestamp, right.ID, right.data[0], right.data[1], right.data[2]);
}

bool operator==(const RCP_4F& left, const RCP_4F& right) {
    return std::tie(left.devclass, left.timestamp, left.ID, left.data[0], left.data[1], left.data[2], left.data[3]) ==
        std::tie(right.devclass, right.timestamp, right.ID, right.data[0], right.data[1], right.data[2], right.data[3]);
}

// Bundle all the structures a host would need for conveniance
struct HostData {
    RCP_TestData testData{};
    RCP_SimpleActuatorData sactData{};
    RCP_BoolData boolData{};

    RCP_PromptDataType ptype = RCP_PromptDataType_RESET;
    std::string pstring;

    uint32_t logtimestamp{};
    std::string log;

    RCP_1F f1{};
    RCP_2F f2{};
    RCP_3F f3{};
    RCP_4F f4{};

    // These are intentionally non-explicit to save typing later
    HostData() = default;
    HostData(RCP_TestData testData, RCP_SimpleActuatorData sactData, RCP_BoolData boolData, RCP_PromptDataType ptype,
             std::string pstring, uint32_t logtimestamp, std::string log, RCP_1F f1, RCP_2F f2, RCP_3F f3, RCP_4F f4) :
        testData(testData), sactData(sactData), boolData(boolData), ptype(ptype), pstring(std::move(pstring)),
        logtimestamp(logtimestamp), log(std::move(log)), f1(f1), f2(f2), f3(f3), f4(f4) {}
    HostData(RCP_TestData testData) : HostData() { this->testData = testData; }
    HostData(RCP_SimpleActuatorData sactData) : HostData() { this->sactData = sactData; }
    HostData(RCP_BoolData boolData) : HostData() { this->boolData = boolData; }
    HostData(RCP_PromptDataType ptype, std::string pstring) : HostData() {
        this->ptype = ptype;
        this->pstring = std::move(pstring);
    }
    HostData(uint32_t logtimestamp, std::string log) : HostData() {
        this->logtimestamp = logtimestamp;
        this->log = std::move(log);
    }
    HostData(RCP_1F f1) : HostData() { this->f1 = f1; }
    HostData(RCP_2F f2) : HostData() { this->f2 = f2; }
    HostData(RCP_3F f3) : HostData() { this->f3 = f3; }
    HostData(RCP_4F f4) : HostData() { this->f4 = f4; }
};

// Test values
// Only works on big endian systems
#define PI 3.1415925f
#define HPI 0xda0f4940

#define PI2 6.283185f
#define HPI2 0xda0fc940

#define PI3 9.4247775f
#define HPI3 0xe3cb1641

#define PI4 12.56637f
#define HPI4 0xda0f4941

#define TS1 0x55555555
#define TS2 0xAAAAAAAA

// Convert one of the above floating point values in hex to a list of comma seperated ones for including in a uint8_t
// array
#define HFLOATARR(value) (uint8_t) (value >> 24), (uint8_t) (value >> 16), (uint8_t) (value >> 8), (uint8_t) value

#define HELLO "HELLO"
#define HELLOHEX 0x48, 0x45, 0x4C, 0x4C, 0x4F
const std::string STR_HELLO = HELLO;

// ------------ SECTION: Checking constants against the spec ------------ //

namespace TEST_Constants {
    TEST(Constants, Constants) {
        // Channels
        EXPECT_EQ(RCP_CH_ZERO, 0x00);
        EXPECT_EQ(RCP_CH_ONE, 0x80);

        // Device classes
        EXPECT_EQ(RCP_DEVCLASS_TEST_STATE, 0x00);
        EXPECT_EQ(RCP_DEVCLASS_SIMPLE_ACTUATOR, 0x01);
        EXPECT_EQ(RCP_DEVCLASS_STEPPER, 0x02);
        EXPECT_EQ(RCP_DEVCLASS_PROMPT, 0x03);
        EXPECT_EQ(RCP_DEVCLASS_ANGLED_ACTUATOR, 0x04);
        EXPECT_EQ(RCP_DEVCLASS_TARGET_LOG, 0x80);
        EXPECT_EQ(RCP_DEVCLASS_AM_PRESSURE, 0x90);
        EXPECT_EQ(RCP_DEVCLASS_TEMPERATURE, 0x91);
        EXPECT_EQ(RCP_DEVCLASS_PRESSURE_TRANSDUCER, 0x92);
        EXPECT_EQ(RCP_DEVCLASS_RELATIVE_HYGROMETER, 0x93);
        EXPECT_EQ(RCP_DEVCLASS_LOAD_CELL, 0x94);
        EXPECT_EQ(RCP_DEVCLASS_BOOL_SENSOR, 0x95);
        EXPECT_EQ(RCP_DEVCLASS_POWERMON, 0xA0);
        EXPECT_EQ(RCP_DEVCLASS_ACCELEROMETER, 0xB0);
        EXPECT_EQ(RCP_DEVCLASS_GYROSCOPE, 0xB1);
        EXPECT_EQ(RCP_DEVCLASS_MAGNETOMETER, 0xB2);
        EXPECT_EQ(RCP_DEVCLASS_GPS, 0xC0);

        // Test state values
        EXPECT_EQ(RCP_TEST_START, 0x00);
        EXPECT_EQ(RCP_TEST_STOP, 0x10);
        EXPECT_EQ(RCP_TEST_PAUSE, 0x11);
        EXPECT_EQ(RCP_DEVICE_RESET, 0x12);
        EXPECT_EQ(RCP_DEVICE_RESET_TIME, 0x13);
        EXPECT_EQ(RCP_DATA_STREAM_STOP, 0x20);
        EXPECT_EQ(RCP_DATA_STREAM_START, 0x21);
        EXPECT_EQ(RCP_TEST_QUERY, 0x30);
        EXPECT_EQ(RCP_HEARTBEATS_CONTROL, 0xF0);

        EXPECT_EQ(RCP_DATA_STREAM_MASK, 0x80);
        EXPECT_EQ(RCP_TEST_STATE_MASK, 0x60);
        EXPECT_EQ(RCP_DEVICE_INITED_MASK, 0x10);
        EXPECT_EQ(RCP_HEARTBEAT_TIME_MASK, 0x0F);

        EXPECT_EQ(RCP_TEST_RUNNING, 0x00);
        EXPECT_EQ(RCP_TEST_STOPPED, 0x20);
        EXPECT_EQ(RCP_TEST_PAUSED, 0x40);
        EXPECT_EQ(RCP_TEST_ESTOP, 0x60);

        // Simple actuator values
        EXPECT_EQ(RCP_SIMPLE_ACTUATOR_OFF, 0x00);
        EXPECT_EQ(RCP_SIMPLE_ACTUATOR_ON, 0x80);
        EXPECT_EQ(RCP_SIMPLE_ACTUATOR_TOGGLE, 0xC0);

        // Stepper control mode values
        EXPECT_EQ(RCP_STEPPER_ABSOLUTE_POS_CONTROL, 0x40);
        EXPECT_EQ(RCP_STEPPER_RELATIVE_POS_CONTROL, 0x80);
        EXPECT_EQ(RCP_STEPPER_SPEED_CONTROL, 0xC0);

        // Prompt values
        EXPECT_EQ(RCP_PromptDataType_GONOGO, 0x00);
        EXPECT_EQ(RCP_PromptDataType_Float, 0x01);
        EXPECT_EQ(RCP_PromptDataType_RESET, 0xFF);

        EXPECT_EQ(RCP_GONOGO_NOGO, 0x00);
        EXPECT_EQ(RCP_GONOGO_GO, 0x01);
    }
} // namespace TEST_Constants

// ------------ SECTION: Library calls before initialization ------------ //

namespace TEST_Preinit {
    // Helper for testing pre-init return values
#define TEST_NONINIT_RUN(function, ...)                                                                                \
    EXPECT_EQ(function(__VA_ARGS__), RCP_ERR_INIT) << #function " does not return init error before open"

    TEST(RCPNoinit, NonInit) {
        ASSERT_FALSE(RCP_isOpen()) << "RCP isOpen returns true when not open";

        TEST_NONINIT_RUN(RCP_shutdown);
        TEST_NONINIT_RUN(RCP_poll);
        TEST_NONINIT_RUN(RCP_sendEStop);
        TEST_NONINIT_RUN(RCP_sendHeartbeat);
        TEST_NONINIT_RUN(RCP_startTest, 0);
        TEST_NONINIT_RUN(RCP_stopTest);
        TEST_NONINIT_RUN(RCP_pauseUnpauseTest);
        TEST_NONINIT_RUN(RCP_deviceReset);
        TEST_NONINIT_RUN(RCP_deviceTimeReset);
        TEST_NONINIT_RUN(RCP_setDataStreaming, false);
        TEST_NONINIT_RUN(RCP_setHeartbeatTime, 0);
        TEST_NONINIT_RUN(RCP_requestTestState);
        TEST_NONINIT_RUN(RCP_sendSimpleActuatorWrite, 0, RCP_SIMPLE_ACTUATOR_TOGGLE);
        TEST_NONINIT_RUN(RCP_sendStepperWrite, 0, RCP_STEPPER_SPEED_CONTROL, 0);
        TEST_NONINIT_RUN(RCP_sendAngledActuatorWrite, 0, 0);
        TEST_NONINIT_RUN(RCP_requestGeneralRead, RCP_DEVCLASS_TEST_STATE, 0);
        TEST_NONINIT_RUN(RCP_requestTareConfiguration, RCP_DEVCLASS_GYROSCOPE, 0, 0, 0);
        TEST_NONINIT_RUN(RCP_promptRespondGONOGO, RCP_GONOGO_GO);
        TEST_NONINIT_RUN(RCP_promptRespondFloat, 0);
    }
} // namespace TEST_Preinit

// ------------ SECTION: Cases when IO does not return properly ------------ //

namespace TEST_BadIO {
    class RCPBadIO : public testing::Test {
        static size_t badSend(const void*, size_t) { return 0; }
        static size_t badRcv(void*, size_t) { return 0; }

    protected:
        RCPBadIO() {
            RCP_LibInitData cbks = CALLBACK_STUBS;
            cbks.sendData = badSend;
            cbks.readData = badRcv;


            RCP_init(cbks);
        }

        ~RCPBadIO() override { RCP_shutdown(); }
    };

#define TEST_BADIOSEND(function, ...)                                                                                  \
    EXPECT_EQ(function(__VA_ARGS__), RCP_ERR_IO_SEND) << #function " does not return IO send error on bad io"
#define TEST_BADIORCV(function, ...)                                                                                   \
    EXPECT_EQ(function(__VA_ARGS__), RCP_ERR_IO_RCV) << #function " does not return IO receive error on bad IO"

    TEST_F(RCPBadIO, BadIO) {
        TEST_BADIORCV(RCP_poll);

        TEST_BADIOSEND(RCP_sendEStop);
        TEST_BADIOSEND(RCP_sendHeartbeat);
        TEST_BADIOSEND(RCP_startTest, 0);
        TEST_BADIOSEND(RCP_stopTest);
        TEST_BADIOSEND(RCP_pauseUnpauseTest);
        TEST_BADIOSEND(RCP_deviceReset);
        TEST_BADIOSEND(RCP_deviceTimeReset);
        TEST_BADIOSEND(RCP_setDataStreaming, false);
        TEST_BADIOSEND(RCP_setHeartbeatTime, 0);
        TEST_BADIOSEND(RCP_requestTestState);
        TEST_BADIOSEND(RCP_sendSimpleActuatorWrite, 0, RCP_SIMPLE_ACTUATOR_TOGGLE);
        TEST_BADIOSEND(RCP_sendStepperWrite, 0, RCP_STEPPER_SPEED_CONTROL, 0);
        TEST_BADIOSEND(RCP_sendAngledActuatorWrite, 0, 0);
        TEST_BADIOSEND(RCP_requestGeneralRead, RCP_DEVCLASS_TEST_STATE, 0);
        TEST_BADIOSEND(RCP_requestTareConfiguration, RCP_DEVCLASS_GYROSCOPE, 0, 0, 0);

        activePromptType = RCP_PromptDataType_GONOGO;
        TEST_BADIOSEND(RCP_promptRespondGONOGO, RCP_GONOGO_GO);

        activePromptType = RCP_PromptDataType_Float;
        TEST_BADIOSEND(RCP_promptRespondFloat, 0);
    }

#undef TEST_BADIOSEND
#undef TEST_BADIORCV
} // namespace TEST_BadIO

// ------------ SECTION: RCP_init ------------ //

namespace TEST_RCP_init {
    TEST(RCP_init, RCPInitWhenAlreadyInited) {
        RCP_init(CALLBACK_STUBS);
        TEST_NONINIT_RUN(RCP_init, CALLBACK_STUBS);
        RCP_shutdown();
    }

    TEST(RCP_init, OpenFalseBeforeInit) { EXPECT_FALSE(RCP_isOpen()); }

    TEST(RCP_init, OpenTrueAfterInit) {
        RCP_init(CALLBACK_STUBS);
        EXPECT_TRUE(RCP_isOpen());
        RCP_shutdown();
    }

#undef TEST_NONINIT_RUN
} // namespace TEST_RCP_init

// ------------ SECTION: RCP_errstr ------------ //

namespace TEST_RCP_errstr {
    TEST(RCPErrstr, RCPErrstrIndexTooLow) { EXPECT_EQ(RCP_errstr(static_cast<RCP_Error>(-1)), nullptr); }
    TEST(RCPErrstr, RCPErrstrIndexTooHigh) { EXPECT_EQ(RCP_errstr(static_cast<RCP_Error>(9)), nullptr); }
} // namespace TEST_RCP_errstr

// ------------ SECTION: RCP_setChannel ------------ //

namespace TEST_RCP_setChannel {
    TEST(Channel, Channel) {
        EXPECT_EQ(channel, RCP_CH_ZERO);

        RCP_setChannel(RCP_CH_ONE);
        EXPECT_EQ(channel, RCP_CH_ONE);
        EXPECT_EQ(RCP_getChannel(), RCP_CH_ONE);

        RCP_setChannel(RCP_CH_ZERO);
        EXPECT_EQ(channel, RCP_CH_ZERO);
        EXPECT_EQ(RCP_getChannel(), RCP_CH_ZERO);
    }
} // namespace TEST_RCP_setChannel

// ------------ SECTION: processIU ------------ //

namespace TEST_processIU {
    struct EnvInfo {
        std::string envName;
        RCP_DeviceClass devclass;
        uint32_t timestamp;
        std::vector<uint8_t> pkt;
        HostData endState;
    };

    class ProcessIU : public testing::Test, public testing::WithParamInterface<EnvInfo> {
        static ProcessIU* ctx;

        static RCP_Error testUpdate(RCP_TestData testData) {
            ctx->hostData.testData = testData;
            return RCP_ERR_SUCCESS;
        }

        static RCP_Error sactUpdate(RCP_SimpleActuatorData sactData) {
            ctx->hostData.sactData = sactData;
            return RCP_ERR_SUCCESS;
        }

        static RCP_Error promptRequest(RCP_PromptInputRequest pir) {
            ctx->hostData.ptype = pir.type;
            ctx->hostData.pstring = std::string(pir.prompt, pir.length);
            return RCP_ERR_SUCCESS;
        }

        static RCP_Error logUpdate(RCP_TargetLogData log) {
            ctx->hostData.logtimestamp = log.timestamp;
            ctx->hostData.log = std::string(log.data, log.length);
            return RCP_ERR_SUCCESS;
        }

        static RCP_Error F1(RCP_1F f1) {
            ctx->hostData.f1 = f1;
            return RCP_ERR_SUCCESS;
        }

        static RCP_Error F2(RCP_2F f2) {
            ctx->hostData.f2 = f2;
            return RCP_ERR_SUCCESS;
        }

        static RCP_Error F3(RCP_3F f3) {
            ctx->hostData.f3 = f3;
            return RCP_ERR_SUCCESS;
        }

        static RCP_Error F4(RCP_4F f4) {
            ctx->hostData.f4 = f4;
            return RCP_ERR_SUCCESS;
        }

        static RCP_Error boolUpdate(RCP_BoolData boolData) {
            ctx->hostData.boolData = boolData;
            return RCP_ERR_SUCCESS;
        }

        static RCP_LibInitData ProcessIUCallbacks;

    public:
        HostData hostData{};

        ProcessIU() {
            ctx = this;
            RCP_init(ProcessIUCallbacks);
        }

        ~ProcessIU() override {
            RCP_shutdown();
            ctx = nullptr;
        }
    };

    ProcessIU* ProcessIU::ctx;
    RCP_LibInitData ProcessIU::ProcessIUCallbacks = {.sendData = SEND_STUB,
                                                     .readData = RCV_STUB,
                                                     .processTestUpdate = testUpdate,
                                                     .processBoolData = boolUpdate,
                                                     .processSimpleActuatorData = sactUpdate,
                                                     .processPromptInput = promptRequest,
                                                     .processTargetLog = logUpdate,
                                                     .processOneFloat = F1,
                                                     .processTwoFloat = F2,
                                                     .processThreeFloat = F3,
                                                     .processFourFloat = F4};

    std::string envToName(const testing::TestParamInfo<ProcessIU::ParamType>& info) { return info.param.envName; }

    TEST_F(ProcessIU, NoCrashOnNullInc) {
        uint8_t pkt[2];
        RCP_Error retval = processIU(RCP_DEVCLASS_TEST_STATE, 0, 0, pkt, nullptr);
        EXPECT_EQ(retval, RCP_ERR_SUCCESS);
    }

    TEST_F(ProcessIU, ErrorOnAmalgamate) {
        RCP_Error retval = processIU(RCP_DEVCLASS_AMALGAMATE, 0, 0, nullptr, nullptr);
        EXPECT_EQ(retval, RCP_ERR_AMALG_NESTING);
    }

    TEST_F(ProcessIU, PromptRequestAmalged) {
        RCP_Error retval = processIU(RCP_DEVCLASS_PROMPT, 0, 0, nullptr, nullptr);
        EXPECT_EQ(retval, RCP_ERR_AMALG_SUBUNIT);
    }

    TEST_F(ProcessIU, TargetLogAmalged) {
        RCP_Error retval = processIU(RCP_DEVCLASS_TARGET_LOG, 0, 0, nullptr, nullptr);
        EXPECT_EQ(retval, RCP_ERR_AMALG_SUBUNIT);
    }

    // These 3 tests can't be included in the parameterized version because it also checks returns of the response
    // functions and if the active prompt type method returns properly
    TEST_F(ProcessIU, PromptRequestReset) {
        uint8_t pkt[] = {RCP_PromptDataType_RESET};

        RCP_Error retval = processIU(RCP_DEVCLASS_PROMPT, 0, 1, pkt, nullptr);

        EXPECT_EQ(retval, RCP_ERR_SUCCESS);
        EXPECT_EQ(RCP_getActivePromptType(), RCP_PromptDataType_RESET);
        EXPECT_TRUE(hostData.pstring.empty());
        EXPECT_EQ(hostData.ptype, RCP_PromptDataType_RESET);

        EXPECT_EQ(RCP_promptRespondFloat(0), RCP_ERR_NO_ACTIVE_PROMPT);
        EXPECT_EQ(RCP_promptRespondGONOGO(RCP_GONOGO_NOGO), RCP_ERR_NO_ACTIVE_PROMPT);
    }

    TEST_F(ProcessIU, PromptRequestFloat) {
        uint8_t pkt[] = {RCP_PromptDataType_Float, HELLOHEX};
        uint16_t pktSize = sizeof(pkt) / sizeof(uint8_t);

        RCP_Error retval = processIU(RCP_DEVCLASS_PROMPT, 0, pktSize, pkt, nullptr);

        EXPECT_EQ(retval, RCP_ERR_SUCCESS);
        EXPECT_EQ(RCP_getActivePromptType(), RCP_PromptDataType_Float);
        EXPECT_EQ(hostData.pstring, STR_HELLO);
        EXPECT_EQ(hostData.ptype, RCP_PromptDataType_Float);

        EXPECT_EQ(RCP_promptRespondGONOGO(RCP_GONOGO_NOGO), RCP_ERR_NO_ACTIVE_PROMPT);
    }

    TEST_F(ProcessIU, PromptRequestGNG) {
        uint8_t pkt[] = {RCP_PromptDataType_GONOGO, HELLOHEX};
        uint16_t pktSize = sizeof(pkt) / sizeof(uint8_t);

        RCP_Error retval = processIU(RCP_DEVCLASS_PROMPT, 0, pktSize, pkt, nullptr);

        EXPECT_EQ(retval, RCP_ERR_SUCCESS);
        EXPECT_EQ(RCP_getActivePromptType(), RCP_PromptDataType_GONOGO);
        EXPECT_EQ(hostData.pstring, STR_HELLO);
        EXPECT_EQ(hostData.ptype, RCP_PromptDataType_GONOGO);

        EXPECT_EQ(RCP_promptRespondFloat(0), RCP_ERR_NO_ACTIVE_PROMPT);
    }

#define CHECKVALS(val) EXPECT_EQ(hostData.val, envInfo.endState.val)
#define CHECKBOOL(val) EXPECT_EQ(static_cast<bool>(hostData.val), static_cast<bool>(envInfo.endState.val))

    TEST_P(ProcessIU, GeneralTests) {
        const EnvInfo& envInfo = GetParam();

        RCP_Error retval =
            processIU(envInfo.devclass, envInfo.timestamp, envInfo.pkt.size(), envInfo.pkt.data(), nullptr);

        EXPECT_EQ(retval, RCP_ERR_SUCCESS);
        // Here we don't use the overloaded equality operators since otherwise googletest wont show which fields were
        // mismatched

        // Test state equality
        CHECKVALS(testData.timestamp);
        CHECKBOOL(testData.dataStreaming);
        CHECKVALS(testData.state);
        CHECKBOOL(testData.isInited);
        CHECKVALS(testData.heartbeatTime);
        CHECKVALS(testData.runningTest);
        CHECKVALS(testData.testProgress);

        // Simple Actuator state equality
        CHECKVALS(sactData.timestamp);
        CHECKVALS(sactData.ID);
        CHECKVALS(sactData.state);

        // Bool Data equality
        CHECKVALS(boolData.timestamp);
        CHECKVALS(boolData.ID);
        CHECKBOOL(boolData.data);

        // Prompt Request data
        CHECKVALS(ptype);
        CHECKVALS(pstring);

        // Log info
        CHECKVALS(logtimestamp);
        CHECKVALS(log);

        // 1F
        CHECKVALS(f1.devclass);
        CHECKVALS(f1.timestamp);
        CHECKVALS(f1.ID);
        CHECKVALS(f1.data);

        // 2F
        CHECKVALS(f2.devclass);
        CHECKVALS(f2.timestamp);
        CHECKVALS(f2.ID);
        CHECKVALS(f2.data[0]);
        CHECKVALS(f2.data[1]);

        // 3F
        CHECKVALS(f3.devclass);
        CHECKVALS(f3.timestamp);
        CHECKVALS(f3.ID);
        CHECKVALS(f3.data[0]);
        CHECKVALS(f3.data[1]);
        CHECKVALS(f3.data[2]);

        // 4F
        CHECKVALS(f4.devclass);
        CHECKVALS(f4.timestamp);
        CHECKVALS(f4.ID);
        CHECKVALS(f4.data[0]);
        CHECKVALS(f4.data[1]);
        CHECKVALS(f4.data[2]);
        CHECKVALS(f4.data[3]);
    }

#undef CHECKVALS
#undef CHECKBOOL

    // clang-format off
    static EnvInfo PTESTS_TESTSTATE[] = {
        EnvInfo{
            .envName = "Nostream_Noinit_Stopped",
            .devclass = RCP_DEVCLASS_TEST_STATE,
            .timestamp = TS1,
            .pkt = {0x20, 0x03},
            .endState = RCP_TestData{
                .timestamp = TS1,
                .dataStreaming = false,
                .state = RCP_TEST_STOPPED,
                .heartbeatTime = 3,
                .runningTest = 0,
                .testProgress = 0
            }
        },
        EnvInfo{
            .envName = "Stream_Init_Paused",
            .devclass = RCP_DEVCLASS_TEST_STATE,
            .timestamp = TS2,
            .pkt = {0xD0, 0x06},
            .endState = RCP_TestData{
                .timestamp = TS2,
                .dataStreaming = true,
                .state = RCP_TEST_PAUSED,
                .isInited = true,
                .heartbeatTime = 6,
                .runningTest = 0,
                .testProgress = 0
            }
        },
        EnvInfo{
            .envName = "Streaming_Inited_Running",
            .devclass = RCP_DEVCLASS_TEST_STATE,
            .timestamp = TS2,
            .pkt = {0x90, 0xF0, 0x01, 0x05},
            .endState = RCP_TestData{
                .timestamp = TS2,
                .dataStreaming = true,
                .state = RCP_TEST_RUNNING,
                .isInited = true,
                .heartbeatTime = 0xF0,
                .runningTest = 1,
                .testProgress = 5
            }
        }
    };

    static EnvInfo PTESTS_SACT[] = {
        EnvInfo{
            .envName = "ActOn",
            .devclass = RCP_DEVCLASS_SIMPLE_ACTUATOR,
            .timestamp = TS1,
            .pkt = {0xF0, RCP_SIMPLE_ACTUATOR_ON},
            .endState = RCP_SimpleActuatorData{
                .timestamp = TS1,
                .ID = 0xF0,
                .state = RCP_SIMPLE_ACTUATOR_ON
            }
        },
        EnvInfo{
            .envName = "ActOff",
            .devclass = RCP_DEVCLASS_SIMPLE_ACTUATOR,
            .timestamp = TS2,
            .pkt = {0x0F, RCP_SIMPLE_ACTUATOR_OFF},
            .endState = RCP_SimpleActuatorData{
                .timestamp = TS2,
                .ID = 0x0F,
                .state = RCP_SIMPLE_ACTUATOR_OFF
            }
        }
    };

    static EnvInfo PTESTS_BOOL[] = {
        EnvInfo{
            .envName = "BoolOn",
            .devclass = RCP_DEVCLASS_BOOL_SENSOR,
            .timestamp = TS1,
            .pkt = {0x0F, 0x80},
            .endState = RCP_BoolData{
                .timestamp = TS1,
                .ID = 0x0F,
                .data = true
            }
        },
        EnvInfo{
            .envName = "BoolOff",
            .devclass = RCP_DEVCLASS_BOOL_SENSOR,
            .timestamp = TS2,
            .pkt = {0xF0, 0x00},
            .endState = RCP_BoolData{
                .timestamp = TS2,
                .ID = 0xF0,
                .data = false
            }
        }
    };

    // These need 4 extra bytes at the end because the packet size is determined from the array length, but there is a
    // subtraction in processIU that is necessary for when called from RCP_poll
    static EnvInfo PTESTS_LOG[] = {
        EnvInfo{
            .envName = "LogHELLO",
            .devclass = RCP_DEVCLASS_TARGET_LOG,
            .timestamp = TS1,
            .pkt = {HELLOHEX, 0, 0, 0, 0},
            .endState = HostData{TS1, STR_HELLO}
        },
        EnvInfo{
            .envName = "LogHELLOHELLO",
            .devclass = RCP_DEVCLASS_TARGET_LOG,
            .timestamp = TS2,
            .pkt = {HELLOHEX, HELLOHEX, 0, 0, 0, 0},
            .endState = HostData{TS2, STR_HELLO + STR_HELLO}
        }
    };

    static EnvInfo PTESTS_xF[] = {
        EnvInfo{
            .envName = "1F_AngledActuator",
            .devclass = RCP_DEVCLASS_ANGLED_ACTUATOR,
            .timestamp = TS1,
            .pkt = {0x00, HFLOATARR(HPI)},
            .endState = RCP_1F{
                .devclass = RCP_DEVCLASS_ANGLED_ACTUATOR,
                .timestamp = TS1,
                .ID = 0,
                .data = PI
            }
        },
        EnvInfo{
            .envName = "1F_AMPressure",
            .devclass = RCP_DEVCLASS_AM_PRESSURE,
            .timestamp = TS1,
            .pkt = {0x01, HFLOATARR(HPI)},
            .endState = RCP_1F{
                .devclass = RCP_DEVCLASS_AM_PRESSURE,
                .timestamp = TS1,
                .ID = 1,
                .data = PI
            }
        },
        EnvInfo{
            .envName = "1F_Temperature",
            .devclass = RCP_DEVCLASS_TEMPERATURE,
            .timestamp = TS1,
            .pkt = {0x02, HFLOATARR(HPI)},
            .endState = RCP_1F{
                .devclass = RCP_DEVCLASS_TEMPERATURE,
                .timestamp = TS1,
                .ID = 2,
                .data = PI
            }
        },
        EnvInfo{
            .envName = "1F_PT",
            .devclass = RCP_DEVCLASS_PRESSURE_TRANSDUCER,
            .timestamp = TS1,
            .pkt = {0x03, HFLOATARR(HPI)},
            .endState = RCP_1F{
                .devclass = RCP_DEVCLASS_PRESSURE_TRANSDUCER,
                .timestamp = TS1,
                .ID = 3,
                .data = PI
            }
        },
        EnvInfo{
            .envName = "1F_Rel_Hygrometer",
            .devclass = RCP_DEVCLASS_RELATIVE_HYGROMETER,
            .timestamp = TS2,
            .pkt = {0x04, HFLOATARR(HPI)},
            .endState = RCP_1F{
                .devclass = RCP_DEVCLASS_RELATIVE_HYGROMETER,
                .timestamp = TS2,
                .ID = 4,
                .data = PI
            }
        },
        EnvInfo{
            .envName = "1F_LoadCell",
            .devclass = RCP_DEVCLASS_LOAD_CELL,
            .timestamp = TS2,
            .pkt = {0x05, HFLOATARR(HPI)},
            .endState = RCP_1F{
                .devclass = RCP_DEVCLASS_LOAD_CELL,
                .timestamp = TS2,
                .ID = 5,
                .data = PI
            }
        },

        EnvInfo{
            .envName = "2F_Stepper",
            .devclass = RCP_DEVCLASS_STEPPER,
            .timestamp = TS1,
            .pkt = {0x01, HFLOATARR(HPI), HFLOATARR(HPI2)},
            .endState = RCP_2F{
                .devclass = RCP_DEVCLASS_STEPPER,
                .timestamp = TS1,
                .ID = 1,
                .data = {PI, PI2}
            }
        },
        EnvInfo{
            .envName = "2F_PowerMonitor",
            .devclass = RCP_DEVCLASS_POWERMON,
            .timestamp = TS2,
            .pkt = {0x05, HFLOATARR(HPI), HFLOATARR(HPI2)},
            .endState = RCP_2F{
                .devclass = RCP_DEVCLASS_POWERMON,
                .timestamp = TS2,
                .ID = 5,
                .data = {PI, PI2}
            }
        },

        EnvInfo{
            .envName = "3F_Accelerometer",
            .devclass = RCP_DEVCLASS_ACCELEROMETER,
            .timestamp = TS1,
            .pkt = {0x01, HFLOATARR(HPI), HFLOATARR(HPI2), HFLOATARR(HPI3)},
            .endState = RCP_3F{
                .devclass = RCP_DEVCLASS_ACCELEROMETER,
                .timestamp = TS1,
                .ID = 1,
                .data = {PI, PI2, PI3}
            }
        },
        EnvInfo{
            .envName = "3F_Gyroscope",
            .devclass = RCP_DEVCLASS_GYROSCOPE,
            .timestamp = TS2,
            .pkt = {0x03, HFLOATARR(HPI), HFLOATARR(HPI2), HFLOATARR(HPI3)},
            .endState = RCP_3F{
                .devclass = RCP_DEVCLASS_GYROSCOPE,
                .timestamp = TS2,
                .ID = 3,
                .data = {PI, PI2, PI3}
            }
        },
        EnvInfo{
            .envName = "3F_Magnetometer",
            .devclass = RCP_DEVCLASS_MAGNETOMETER,
            .timestamp = TS2,
            .pkt = {0x04, HFLOATARR(HPI), HFLOATARR(HPI2), HFLOATARR(HPI3)},
            .endState = RCP_3F{
                .devclass = RCP_DEVCLASS_MAGNETOMETER,
                .timestamp = TS2,
                .ID = 4,
                .data = {PI, PI2, PI3}
            }
        },

        EnvInfo{
            .envName = "4F_GPS",
            .devclass = RCP_DEVCLASS_GPS,
            .timestamp = TS1,
            .pkt = {0x01, HFLOATARR(HPI), HFLOATARR(HPI2), HFLOATARR(HPI3), HFLOATARR(HPI4)},
            .endState = RCP_4F{
                .devclass = RCP_DEVCLASS_GPS,
                .timestamp = TS1,
                .ID = 1,
                .data = {PI, PI2, PI3, PI4}
            }
        },
    };
    // clang-format on

    INSTANTIATE_TEST_SUITE_P(TestState, ProcessIU, testing::ValuesIn(PTESTS_TESTSTATE), envToName);
    INSTANTIATE_TEST_SUITE_P(SimpleActuator, ProcessIU, testing::ValuesIn(PTESTS_SACT), envToName);
    INSTANTIATE_TEST_SUITE_P(BoolSensor, ProcessIU, testing::ValuesIn(PTESTS_BOOL), envToName);
    INSTANTIATE_TEST_SUITE_P(TargetLog, ProcessIU, testing::ValuesIn(PTESTS_LOG), envToName);
    INSTANTIATE_TEST_SUITE_P(xF, ProcessIU, testing::ValuesIn(PTESTS_xF), envToName);
} // namespace TEST_processIU

// ------------ SECTION: RCP_poll ------------ //

namespace TEST_RCP_poll {
    struct EnvInfo {
        std::string envName;
        std::vector<uint8_t> pkt;
        HostData endState;
    };

    class RCPPoll : public testing::Test, public testing::WithParamInterface<EnvInfo> {
        static RCPPoll* ctx;

        static size_t readData(void* buffer, size_t len) {
            auto* buf = static_cast<uint8_t*>(buffer);
            size_t i = 0;
            for(; i < len && !ctx->pkt.isEmpty(); i++) buf[i] = ctx->pkt.pop();
            return i;
        }

        static RCP_Error testUpdate(RCP_TestData testData) {
            ctx->hostData.testData = testData;
            return RCP_ERR_SUCCESS;
        }

        static RCP_Error sactUpdate(RCP_SimpleActuatorData sactData) {
            ctx->hostData.sactData = sactData;
            return RCP_ERR_SUCCESS;
        }

        static RCP_Error promptRequest(RCP_PromptInputRequest pir) {
            ctx->hostData.ptype = pir.type;
            ctx->hostData.pstring = std::string(pir.prompt, pir.length);
            return RCP_ERR_SUCCESS;
        }

        static RCP_Error logUpdate(RCP_TargetLogData log) {
            ctx->hostData.logtimestamp = log.timestamp;
            ctx->hostData.log = std::string(log.data, log.length);
            return RCP_ERR_SUCCESS;
        }

        static RCP_Error F1(RCP_1F f1) {
            ctx->hostData.f1 = f1;
            return RCP_ERR_SUCCESS;
        }

        static RCP_Error F2(RCP_2F f2) {
            ctx->hostData.f2 = f2;
            return RCP_ERR_SUCCESS;
        }

        static RCP_Error F3(RCP_3F f3) {
            ctx->hostData.f3 = f3;
            return RCP_ERR_SUCCESS;
        }

        static RCP_Error F4(RCP_4F f4) {
            ctx->hostData.f4 = f4;
            return RCP_ERR_SUCCESS;
        }

        static RCP_Error boolUpdate(RCP_BoolData boolData) {
            ctx->hostData.boolData = boolData;
            return RCP_ERR_SUCCESS;
        }

        static RCP_LibInitData RCPPollCallbacks;

    public:
        LRI::RCI::RingBuffer<uint8_t> pkt{256};
        HostData hostData;

        RCPPoll() {
            ctx = this;
            RCP_init(RCPPollCallbacks);
        }

        ~RCPPoll() override {
            RCP_shutdown();
            ctx = nullptr;
        }
    };

    RCPPoll* RCPPoll::ctx;
    RCP_LibInitData RCPPoll::RCPPollCallbacks = {.sendData = SEND_STUB,
                                                 .readData = readData,
                                                 .processTestUpdate = testUpdate,
                                                 .processBoolData = boolUpdate,
                                                 .processSimpleActuatorData = sactUpdate,
                                                 .processPromptInput = promptRequest,
                                                 .processTargetLog = logUpdate,
                                                 .processOneFloat = F1,
                                                 .processTwoFloat = F2,
                                                 .processThreeFloat = F3,
                                                 .processFourFloat = F4};


    std::string envToName(const testing::TestParamInfo<RCPPoll::ParamType>& info) { return info.param.envName; }

    TEST_F(RCPPoll, DiscardZeroLengthPacketsCompact) {
        pkt.push(0);
        EXPECT_EQ(RCP_poll(), RCP_ERR_SUCCESS);
    }

    TEST_F(RCPPoll, PollOnEmptyBuffer) { EXPECT_EQ(RCP_poll(), RCP_ERR_IO_RCV); }

    TEST_F(RCPPoll, PollIncompletePacket) {
        pkt.push(0x05);

        EXPECT_EQ(RCP_poll(), RCP_ERR_IO_RCV);
    }

    TEST_F(RCPPoll, AmalgErrorOnLog) {
        uint8_t bytes[] = {0x0A, RCP_DEVCLASS_AMALGAMATE, 0x00, 0x00, 0x00, 0x00, RCP_DEVCLASS_TARGET_LOG, HELLOHEX};
        for(const auto& b : bytes) pkt.push(b);

        EXPECT_EQ(RCP_poll(), RCP_ERR_AMALG_SUBUNIT);
        EXPECT_TRUE(pkt.isEmpty());
        EXPECT_TRUE(hostData.log.empty());
    }

    TEST_F(RCPPoll, AmalgErrorOnPrompt) {
        uint8_t bytes[] = {
            0x0B,    RCP_DEVCLASS_AMALGAMATE, 0x00, 0x00, 0x00, 0x00, RCP_DEVCLASS_PROMPT, RCP_PromptDataType_Float,
            HELLOHEX};
        for(const auto& b : bytes) pkt.push(b);

        EXPECT_EQ(RCP_poll(), RCP_ERR_AMALG_SUBUNIT);
        EXPECT_TRUE(pkt.isEmpty());
        EXPECT_TRUE(hostData.pstring.empty());
    }

    TEST_F(RCPPoll, AmalgErrorOnNesting) {
        uint8_t bytes[] = {0x0C, RCP_DEVCLASS_AMALGAMATE, 0x00, 0x00, 0x00,
                           0x00, RCP_DEVCLASS_AMALGAMATE, 0x00, 0x00, 0x00,
                           0x00, RCP_DEVCLASS_TEST_STATE, 0xD0, 0x05};
        for(const auto& b : bytes) pkt.push(b);

        EXPECT_EQ(RCP_poll(), RCP_ERR_AMALG_NESTING);
        EXPECT_TRUE(pkt.isEmpty());
        EXPECT_EQ(hostData.testData, RCP_TestData{});
    }

#define CHECKVALS(val) EXPECT_EQ(hostData.val, envInfo.endState.val)
#define CHECKBOOL(val) EXPECT_EQ(static_cast<bool>(hostData.val), static_cast<bool>(envInfo.endState.val))

    TEST_P(RCPPoll, GeneralTests) {
        const EnvInfo& envInfo = GetParam();
        for(const uint8_t& b : envInfo.pkt) pkt.push(b);

        RCP_Error retval = RCP_poll();

        EXPECT_EQ(retval, RCP_ERR_SUCCESS);
        EXPECT_TRUE(pkt.isEmpty());

        // Here we don't use the overloaded equality operators since otherwise googletest wont show which fields were
        // mismatched

        // Test state equality
        CHECKVALS(testData.timestamp);
        CHECKBOOL(testData.dataStreaming);
        CHECKVALS(testData.state);
        CHECKBOOL(testData.isInited);
        CHECKVALS(testData.heartbeatTime);
        CHECKVALS(testData.runningTest);
        CHECKVALS(testData.testProgress);

        // Simple Actuator state equality
        CHECKVALS(sactData.timestamp);
        CHECKVALS(sactData.ID);
        CHECKVALS(sactData.state);

        // Bool Data equality
        CHECKVALS(boolData.timestamp);
        CHECKVALS(boolData.ID);
        CHECKBOOL(boolData.data);

        // Prompt Request data
        CHECKVALS(ptype);
        CHECKVALS(pstring);

        // Log info
        CHECKVALS(logtimestamp);
        CHECKVALS(log);

        // 1F
        CHECKVALS(f1.devclass);
        CHECKVALS(f1.timestamp);
        CHECKVALS(f1.ID);
        CHECKVALS(f1.data);

        // 2F
        CHECKVALS(f2.devclass);
        CHECKVALS(f2.timestamp);
        CHECKVALS(f2.ID);
        CHECKVALS(f2.data[0]);
        CHECKVALS(f2.data[1]);

        // 3F
        CHECKVALS(f3.devclass);
        CHECKVALS(f3.timestamp);
        CHECKVALS(f3.ID);
        CHECKVALS(f3.data[0]);
        CHECKVALS(f3.data[1]);
        CHECKVALS(f3.data[2]);

        // 4F
        CHECKVALS(f4.devclass);
        CHECKVALS(f4.timestamp);
        CHECKVALS(f4.ID);
        CHECKVALS(f4.data[0]);
        CHECKVALS(f4.data[1]);
        CHECKVALS(f4.data[2]);
        CHECKVALS(f4.data[3]);
    }

#undef CHECKVALS
#undef CHECKBOOL

    // clang-format off
    static EnvInfo PTESTS_BASIC_COMPACT[] = {
        EnvInfo{
            .envName = "PollWrongChannel",
            .pkt = {RCP_CH_ONE | 0x06, RCP_DEVCLASS_TEST_STATE, HFLOATARR(TS1), 0xD0, 0xFF},
            .endState = HostData{}
        },
        EnvInfo{
            .envName = "Compact_Basic_TestState",
            .pkt = {0x06, RCP_DEVCLASS_TEST_STATE, HFLOATARR(TS1), 0xD0, 0xFF},
            .endState = RCP_TestData{
                .timestamp = TS1,// 1101
                .dataStreaming = true,
                .state = RCP_TEST_PAUSED,
                .isInited = true,
                .heartbeatTime = 0xFF,
                .runningTest = 0,
                .testProgress = 0
            }
        },
        EnvInfo{
            .envName = "Compact_Basic_1F",
            .pkt = {0x09, RCP_DEVCLASS_AM_PRESSURE, HFLOATARR(TS1), 0x05, HFLOATARR(HPI)},
            .endState = RCP_1F{
                .devclass = RCP_DEVCLASS_AM_PRESSURE,
                .timestamp = TS1,
                .ID = 5,
                .data = PI
            }
        },
        EnvInfo{
            .envName = "Compact_Basic_Log",
            .pkt = {0x09, RCP_DEVCLASS_TARGET_LOG, HFLOATARR(TS2), HELLOHEX},
            .endState = HostData{TS2, STR_HELLO}
        },
        EnvInfo{ // This one can be fixed by not always incrementing head by 4 on line 296 after timestamp extract
            .envName = "Compact_Basic_Prompt",
            .pkt = {0x06, RCP_DEVCLASS_PROMPT, RCP_PromptDataType_Float, HELLOHEX},
            .endState = HostData{RCP_PromptDataType_Float, STR_HELLO}
        }
    };

    static EnvInfo PTESTS_BASIC_EXTENDED[] = {
        EnvInfo{
            .envName = "Extended_Basic_TestState",
            .pkt = {0x40, 0x00, 0x05, RCP_DEVCLASS_TEST_STATE, HFLOATARR(TS1), 0xD0, 0xFF},
            .endState = RCP_TestData{
                .timestamp = TS1,// 1101
                .dataStreaming = true,
                .state = RCP_TEST_PAUSED,
                .isInited = true,
                .heartbeatTime = 0xFF,
                .runningTest = 0,
                .testProgress = 0
            }
        },
        EnvInfo{
            .envName = "Extended_Basic_1F",
            .pkt = {0x40, 0x00, 0x08, RCP_DEVCLASS_AM_PRESSURE, HFLOATARR(TS1), 0x05, HFLOATARR(HPI)},
            .endState = RCP_1F{
                .devclass = RCP_DEVCLASS_AM_PRESSURE,
                .timestamp = TS1,
                .ID = 5,
                .data = PI
            }
        },
        EnvInfo{
            .envName = "Extended_Basic_Log",
            .pkt = {0x40, 0x00, 0x08, RCP_DEVCLASS_TARGET_LOG, HFLOATARR(TS2), HELLOHEX},
            .endState = HostData{TS2, STR_HELLO}
        },
        EnvInfo{ // This one can be fixed by not always incrementing head by 4 on line 296 after timestamp extract
            .envName = "Extended_Basic_Prompt",
            .pkt = {0x40, 0x00, 0x05, RCP_DEVCLASS_PROMPT, RCP_PromptDataType_Float, HELLOHEX},
            .endState = HostData{RCP_PromptDataType_Float, STR_HELLO}
        }
    };

    static EnvInfo PTESTS_AMALG[] = {
        EnvInfo{
            .envName = "Amalged_Floats",
            .pkt = {0x34, RCP_DEVCLASS_AMALGAMATE, HFLOATARR(TS1),
                RCP_DEVCLASS_AM_PRESSURE, 0x0F, HFLOATARR(HPI),
                RCP_DEVCLASS_POWERMON, 0x01, HFLOATARR(HPI), HFLOATARR(HPI2),
                RCP_DEVCLASS_ACCELEROMETER, 0x05, HFLOATARR(HPI), HFLOATARR(HPI2), HFLOATARR(HPI3),
                RCP_DEVCLASS_GPS, 0x00, HFLOATARR(HPI), HFLOATARR(HPI2), HFLOATARR(HPI3), HFLOATARR(HPI4)},
            .endState = HostData{
                RCP_TestData{}, RCP_SimpleActuatorData{}, RCP_BoolData{}, RCP_PromptDataType_RESET, "", 0, "",
                RCP_1F{
                    .devclass = RCP_DEVCLASS_AM_PRESSURE,
                    .timestamp = TS1,
                    .ID = 0x0F,
                    .data = PI
                },
                RCP_2F{
                    .devclass = RCP_DEVCLASS_POWERMON,
                    .timestamp = TS1,
                    .ID = 1,
                    .data = {PI, PI2}
                },
                RCP_3F{
                    .devclass = RCP_DEVCLASS_ACCELEROMETER,
                    .timestamp = TS1,
                    .ID = 5,
                    .data = {PI, PI2, PI3}
                },
                RCP_4F{
                    .devclass = RCP_DEVCLASS_GPS,
                    .timestamp = TS1,
                    .ID = 0,
                    .data = {PI, PI2, PI3, PI4}
                }
            }
        },
        // In reality this packet can fit in a compact, but whatever. Amalged packets will likely need the extended
        // format because it can send multiple IDs of one devclass
        EnvInfo{
            .envName = "Amalged_Longer",
            .pkt = {0x40, 0x00, 0x3E, RCP_DEVCLASS_AMALGAMATE, HFLOATARR(TS2), // 4
                RCP_DEVCLASS_TEST_STATE, 0x90, 0x05, 0xF0, 0x0F, // 5
                RCP_DEVCLASS_SIMPLE_ACTUATOR, 0x05, RCP_SIMPLE_ACTUATOR_ON, // 3
                RCP_DEVCLASS_BOOL_SENSOR, 0x03, RCP_SIMPLE_ACTUATOR_ON, // 3
                // Float packets are in a different order here to test the offsetting for GPS
                RCP_DEVCLASS_GPS, 0x00, HFLOATARR(HPI), HFLOATARR(HPI2), HFLOATARR(HPI3), HFLOATARR(HPI4), // 18
                RCP_DEVCLASS_ANGLED_ACTUATOR, 0x0F, HFLOATARR(HPI), // 6
                RCP_DEVCLASS_STEPPER, 0x01, HFLOATARR(HPI), HFLOATARR(HPI2), // 10
                RCP_DEVCLASS_GYROSCOPE, 0x05, HFLOATARR(HPI), HFLOATARR(HPI2), HFLOATARR(HPI3) // 14
            },
            .endState = HostData{
                RCP_TestData{
                    .timestamp = TS2,
                    .dataStreaming = true,
                    .state = RCP_TEST_RUNNING,
                    .isInited = true,
                    .heartbeatTime = 5,
                    .runningTest = 0xF0,
                    .testProgress = 0x0F
                },
                RCP_SimpleActuatorData{
                    .timestamp = TS2,
                    .ID = 5,
                    .state = RCP_SIMPLE_ACTUATOR_ON
                },
                RCP_BoolData{
                    .timestamp = TS2,
                    .ID = 3,
                    .data = true
                },
                RCP_PromptDataType_RESET, "",
                0, "",
                RCP_1F{
                    .devclass = RCP_DEVCLASS_ANGLED_ACTUATOR,
                    .timestamp = TS2,
                    .ID = 0x0F,
                    .data = PI
                },
                RCP_2F{
                    .devclass = RCP_DEVCLASS_STEPPER,
                    .timestamp = TS2,
                    .ID = 1,
                    .data = {PI, PI2}
                },
                RCP_3F{
                    .devclass = RCP_DEVCLASS_GYROSCOPE,
                    .timestamp = TS2,
                    .ID = 5,
                    .data = {PI, PI2, PI3}
                },
                RCP_4F{
                    .devclass = RCP_DEVCLASS_GPS,
                    .timestamp = TS2,
                    .ID = 0,
                    .data = {PI, PI2, PI3, PI4}
                }
            }
        }
    };
    // clang-format on

    INSTANTIATE_TEST_SUITE_P(BasicCompactPackets, RCPPoll, testing::ValuesIn(PTESTS_BASIC_COMPACT), envToName);
    INSTANTIATE_TEST_SUITE_P(BasicExtendedPackets, RCPPoll, testing::ValuesIn(PTESTS_BASIC_EXTENDED), envToName);
    INSTANTIATE_TEST_SUITE_P(AmalgedCompactPackets, RCPPoll, testing::ValuesIn(PTESTS_AMALG), envToName);


} // namespace TEST_RCP_poll

// ------------ SECTION: Various sender functions ------------ //

namespace TEST_RCP_Senders {
    struct EnvInfo {
        std::string envName;
        std::vector<uint8_t> endState;
        RCP_Error (*function)();
    };

    class RCPSenders : public testing::Test, public testing::WithParamInterface<EnvInfo> {
        static RCPSenders* ctx;

        static size_t sendData(const void* data, size_t len) {
            const auto* pkt = static_cast<const uint8_t*>(data);
            ctx->pkt.insert(ctx->pkt.end(), pkt, pkt + len);
            return len;
        }

        static RCP_LibInitData RCPSendersCallbacks;

    public:
        std::vector<uint8_t> pkt;

        RCPSenders() {
            ctx = this;
            RCP_init(RCPSendersCallbacks);
        }

        ~RCPSenders() override {
            RCP_shutdown();
            ctx = nullptr;
        }
    };

    RCPSenders* RCPSenders::ctx;
    RCP_LibInitData RCPSenders::RCPSendersCallbacks = {.sendData = sendData,
                                                       .readData = RCV_STUB,
                                                       .processTestUpdate = TEST_STUB,
                                                       .processBoolData = BOOL_STUB,
                                                       .processSimpleActuatorData = SACT_STUB,
                                                       .processPromptInput = PROMPT_STUB,
                                                       .processTargetLog = LOG_STUB,
                                                       .processOneFloat = F1_STUB,
                                                       .processTwoFloat = F2_STUB,
                                                       .processThreeFloat = F3_STUB,
                                                       .processFourFloat = F4_STUB};

    std::string envToName(const testing::TestParamInfo<RCPSenders::ParamType>& info) { return info.param.envName; }

    TEST_F(RCPSenders, RejectReadsForInvalidDevclasses) {
        RCP_Error retval = RCP_requestGeneralRead(RCP_DEVCLASS_PROMPT, 0);
        EXPECT_EQ(retval, RCP_ERR_INVALID_DEVCLASS);

        retval = RCP_requestGeneralRead(RCP_DEVCLASS_TARGET_LOG, 0);
        EXPECT_EQ(retval, RCP_ERR_INVALID_DEVCLASS);

        retval = RCP_requestGeneralRead(RCP_DEVCLASS_AMALGAMATE, 0);
        EXPECT_EQ(retval, RCP_ERR_INVALID_DEVCLASS);
    }

    TEST_F(RCPSenders, RejectTaresForInvalidDevclasses) {
        RCP_Error retval = RCP_requestTareConfiguration(RCP_DEVCLASS_TEST_STATE, 0, 0, 0);
        EXPECT_EQ(retval, RCP_ERR_INVALID_DEVCLASS);

        retval = RCP_requestTareConfiguration(RCP_DEVCLASS_SIMPLE_ACTUATOR, 0, 0, 0);
        EXPECT_EQ(retval, RCP_ERR_INVALID_DEVCLASS);

        retval = RCP_requestTareConfiguration(RCP_DEVCLASS_STEPPER, 0, 0, 0);
        EXPECT_EQ(retval, RCP_ERR_INVALID_DEVCLASS);

        retval = RCP_requestTareConfiguration(RCP_DEVCLASS_PROMPT, 0, 0, 0);
        EXPECT_EQ(retval, RCP_ERR_INVALID_DEVCLASS);

        retval = RCP_requestTareConfiguration(RCP_DEVCLASS_ANGLED_ACTUATOR, 0, 0, 0);
        EXPECT_EQ(retval, RCP_ERR_INVALID_DEVCLASS);

        retval = RCP_requestTareConfiguration(RCP_DEVCLASS_TARGET_LOG, 0, 0, 0);
        EXPECT_EQ(retval, RCP_ERR_INVALID_DEVCLASS);

        retval = RCP_requestTareConfiguration(RCP_DEVCLASS_BOOL_SENSOR, 0, 0, 0);
        EXPECT_EQ(retval, RCP_ERR_INVALID_DEVCLASS);

        retval = RCP_requestTareConfiguration(RCP_DEVCLASS_AMALGAMATE, 0, 0, 0);
        EXPECT_EQ(retval, RCP_ERR_INVALID_DEVCLASS);
    }

    TEST_F(RCPSenders, PromptResponseFloat) {
        const std::vector<uint8_t> endState = {0x04, RCP_DEVCLASS_PROMPT, HFLOATARR(HPI)};

        activePromptType = RCP_PromptDataType_Float;
        RCP_Error retval = RCP_promptRespondFloat(PI);

        EXPECT_EQ(retval, RCP_ERR_SUCCESS);
        ASSERT_EQ(pkt.size(), endState.size());

        for(size_t i = 0; i < pkt.size(); i++) {
            EXPECT_EQ(pkt[i], endState[i]) << "Bytes at " << i << " are not equal!";
        }
    }

    TEST_F(RCPSenders, PromptResponseGNG) {
        const std::vector<uint8_t> endState = {0x01, RCP_DEVCLASS_PROMPT, RCP_GONOGO_GO};

        activePromptType = RCP_PromptDataType_GONOGO;
        RCP_Error retval = RCP_promptRespondGONOGO(RCP_GONOGO_GO);

        EXPECT_EQ(retval, RCP_ERR_SUCCESS);
        ASSERT_EQ(pkt.size(), endState.size());

        for(size_t i = 0; i < pkt.size(); i++) {
            EXPECT_EQ(pkt[i], endState[i]) << "Bytes at " << i << " are not equal!";
        }
    }

    TEST_P(RCPSenders, SendTests) {
        const EnvInfo& envInfo = GetParam();

        RCP_Error retval = envInfo.function();

        EXPECT_EQ(retval, RCP_ERR_SUCCESS);
        ASSERT_EQ(pkt.size(), envInfo.endState.size());

        for(size_t i = 0; i < pkt.size(); i++) {
            EXPECT_EQ(pkt[i], envInfo.endState[i]) << "Bytes at " << i << " are not equal!";
        }
    }

    // clang-format off
    static EnvInfo PTESTS_SENDERS[] = {
        EnvInfo{
            .envName = "Estop",
            .endState = {0x00},
            .function = []{ return RCP_sendEStop(); }
        },
        EnvInfo{
            .envName = "Heartbeat",
            .endState = {0x01, RCP_DEVCLASS_TEST_STATE, RCP_HEARTBEAT},
            .function = [] { return RCP_sendHeartbeat(); }
        },
        EnvInfo{
            .envName = "StartTest",
            .endState = {0x02, RCP_DEVCLASS_TEST_STATE, RCP_TEST_START, 0x05},
            .function = [] { return RCP_startTest(5); }
        },
        EnvInfo{
            .envName = "StopTest",
            .endState = {0x01, RCP_DEVCLASS_TEST_STATE, RCP_TEST_STOP},
            .function = [] { return RCP_stopTest(); }
        },
        EnvInfo{
            .envName = "PauseUnpauseTest",
            .endState = {0x01, RCP_DEVCLASS_TEST_STATE, RCP_TEST_PAUSE},
            .function = [] { return RCP_pauseUnpauseTest(); }
        },
        EnvInfo{
            .envName = "DeviceReset",
            .endState = {0x01, RCP_DEVCLASS_TEST_STATE, RCP_DEVICE_RESET},
            .function = [] { return RCP_deviceReset(); }
        },
        EnvInfo{
            .envName = "TimeReset",
            .endState = {0x01, RCP_DEVCLASS_TEST_STATE, RCP_DEVICE_RESET_TIME},
            .function = [] { return RCP_deviceTimeReset(); }
        },
        EnvInfo{
            .envName = "DataStreamingOff",
            .endState = {0x01, RCP_DEVCLASS_TEST_STATE, RCP_DATA_STREAM_STOP},
            .function = [] { return RCP_setDataStreaming(false); }
        },
        EnvInfo{
            .envName = "DataStreamingOn",
            .endState = {0x01, RCP_DEVCLASS_TEST_STATE, RCP_DATA_STREAM_START},
            .function = [] { return RCP_setDataStreaming(true); }
        },
        EnvInfo{
            .envName = "SetHeartbeatTime",
            .endState = {0x02, RCP_DEVCLASS_TEST_STATE, RCP_HEARTBEATS_CONTROL, 6},
            .function = [] { return RCP_setHeartbeatTime(6); }
        },
        EnvInfo{
            .envName = "RequestTestState",
            .endState = {0x01, RCP_DEVCLASS_TEST_STATE, RCP_TEST_QUERY},
            .function = [] { return RCP_requestTestState(); }
        },
        EnvInfo{
            .envName = "SimpleActuatorWrite",
            .endState = {0x02, RCP_DEVCLASS_SIMPLE_ACTUATOR, 0x05, RCP_SIMPLE_ACTUATOR_ON},
            .function = [] { return RCP_sendSimpleActuatorWrite(5, RCP_SIMPLE_ACTUATOR_ON); }
        },
        EnvInfo{
            .envName = "StepperWrite",
            .endState = {0x06, RCP_DEVCLASS_STEPPER, 0x17, RCP_STEPPER_SPEED_CONTROL, HFLOATARR(HPI)},
            .function = [] { return RCP_sendStepperWrite(0x17, RCP_STEPPER_SPEED_CONTROL, PI); }
        },
        EnvInfo{
            .envName = "AngledActuatorWrite",
            .endState = {0x05, RCP_DEVCLASS_ANGLED_ACTUATOR, 0x10, HFLOATARR(HPI)},
            .function = [] { return RCP_sendAngledActuatorWrite(0x10, PI); }
        },
        EnvInfo{
            .envName = "GeneralReadRequest",
            .endState = {0x01, RCP_DEVCLASS_PRESSURE_TRANSDUCER, 0x55},
            .function = [] { return RCP_requestGeneralRead(RCP_DEVCLASS_PRESSURE_TRANSDUCER, 0x55); }
        },
        EnvInfo{
            .envName = "TareRequest",
            .endState = {0x06, RCP_DEVCLASS_ACCELEROMETER, 1, 2, HFLOATARR(HPI2)},
            .function = [] { return RCP_requestTareConfiguration(RCP_DEVCLASS_ACCELEROMETER, 1, 2, PI2); }
        }
    };
    // clang-format on

    INSTANTIATE_TEST_SUITE_P(CheckOutputs, RCPSenders, testing::ValuesIn(PTESTS_SENDERS), envToName);
} // namespace TEST_RCP_Senders
