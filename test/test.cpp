#include "RCP_Host/RCP_Host.h"
#include "RingBuffer.h"
#include "fixtures.h"
#include "gtest/gtest.h"

RCP* context;

// Since the data needed in context is different depending on what we are doing, here are a bunch of helpers so
// casting doesnt have to be done all over the place
#define IN dynamic_cast<RCPIn*>(context)->inbuffer
#define OUT dynamic_cast<RCPOut*>(context)->outbuffer
#define TESTDATA dynamic_cast<RCPTestState*>(context)->testData
#define BOOLDATA dynamic_cast<RCPBoolData*>(context)->bdata
#define SACTD dynamic_cast<RCPSactD*>(context)->sactd
#define PIR dynamic_cast<RCPPromptInputRequest*>(context)->pir
#define CDATA dynamic_cast<RCPCustomData*>(context)->cdata
#define FLOATS dynamic_cast<RCPFloats*>(context)

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

#define HFLOATARR(value) (uint8_t)(value >> 24), (uint8_t)(value >> 16), (uint8_t)(value >> 8), (uint8_t)value

#define HELLO "HELLO"
#define HELLOHEX 0x48, 0x45, 0x4C, 0x4C, 0x4F

size_t sendData(const void* rdata, size_t length) {
    const auto* data = static_cast<const uint8_t*>(rdata);
    int i = 0;
    for(; i < length && !OUT.isFull(); i++) {
        OUT.push(data[i]);
    }
    return i;
}

size_t readData(void* rdata, size_t length) {
    uint8_t* data = static_cast<uint8_t*>(rdata);
    int i = 0;
    for(; i < length && !IN.isEmpty(); i++) {
        data[i] = IN.pop();
    }

    return i;
}

// For testing bad IO returns
size_t badSend([[maybe_unused]] const void* rdata, [[maybe_unused]] size_t length) { return 0; }

size_t badRead([[maybe_unused]] void* rdata, [[maybe_unused]] size_t length) { return 0; }

int processTestUpdate(RCP_TestData testData) {
    TESTDATA = testData;
    return 0;
}

int processBoolData(RCP_BoolData bdata) {
    BOOLDATA = bdata;
    return 0;
}

int processSactD(RCP_SimpleActuatorData sactd) {
    SACTD = sactd;
    return 0;
}

int processPIR(RCP_PromptInputRequest pir) {
    PIR = pir;
    return 0;
}

int processCustom(RCP_CustomData cdata) {
    CDATA = cdata;
    return 0;
}

int processFloat1(RCP_OneFloat float1) {
    FLOATS->float1 = float1;
    return 0;
}

int processFloat2(RCP_TwoFloat float2) {
    FLOATS->float2 = float2;
    return 0;
}

int processFloat3(RCP_ThreeFloat float3) {
    FLOATS->float3 = float3;
    return 0;
}

int processFloat4(RCP_FourFloat float4) {
    FLOATS->float4 = float4;
    return 0;
}

RCP_LibInitData initData{
    .sendData = sendData,
    .readData = readData,
    .processTestUpdate = processTestUpdate,
    .processBoolData = processBoolData,
    .processSimpleActuatorData = processSactD,
    .processPromptInput = processPIR,
    .processSerialData = processCustom,
    .processOneFloat = processFloat1,
    .processTwoFloat = processFloat2,
    .processThreeFloat = processFloat3,
    .processFourFloat = processFloat4,
};

// Helper for testing pre-init return values
#define TEST_NONINIT_RUN(function, ...)                                                                                \
    EXPECT_EQ(function(__VA_ARGS__), -1) << #function " does not return -1 before open"

// Helper for pushing values to the input buffer
#define PUSH(value) IN.push(value)

// Helper to push the test string to the inbuffer
#define PUSHHELLO()                                                                                                    \
    do {                                                                                                               \
        uint8_t vals[] = {HELLOHEX};                                                                                   \
        for(int i = 0; i < sizeof(vals); i++)                                                                          \
            PUSH(vals[i]);                                                                                             \
    }                                                                                                                  \
    while(0)

// Helper to pop values out of the input buffer
#define POP() OUT.pop()

// ------------ SECTION: Cases outside RCP initialization ------------ //

TEST(RCPNoinit, NonInit) {
    ASSERT_FALSE(RCP_isOpen()) << "RCP isOpen returns true when not open";

    TEST_NONINIT_RUN(RCP_shutdown);
    TEST_NONINIT_RUN(RCP_poll);
    TEST_NONINIT_RUN(RCP_sendEStop);
    TEST_NONINIT_RUN(RCP_sendHeartbeat);
    TEST_NONINIT_RUN(RCP_sendSimpleActuatorWrite, 0, RCP_SIMPLE_ACTUATOR_TOGGLE);
    TEST_NONINIT_RUN(RCP_sendStepperWrite, 0, RCP_STEPPER_SPEED_CONTROL, 0);
    TEST_NONINIT_RUN(RCP_requestAngledActuatorWrite, 0, 0);
    TEST_NONINIT_RUN(RCP_requestGeneralRead, RCP_DEVCLASS_TEST_STATE, 0);
    TEST_NONINIT_RUN(RCP_requestTareConfiguration, RCP_DEVCLASS_GYROSCOPE, 0, 0, 0);
    TEST_NONINIT_RUN(RCP_promptRespondGONOGO, RCP_GONOGO_GO);
    TEST_NONINIT_RUN(RCP_promptRespondFloat, 0);
    TEST_NONINIT_RUN(RCP_sendRawSerial, nullptr, 0);
}

#define TEST_BADIO(function, ...) EXPECT_EQ(function(__VA_ARGS__), -2) << #function " does not return -2 on bad io"

TEST(RCPBadIO, BadIO) {
    RCP_LibInitData badinit{badSend, badRead};
    RCP_init(badinit);
    RCP_setChannel(RCP_CH_ZERO);

    TEST_BADIO(RCP_poll);
    TEST_BADIO(RCP_sendEStop);
    TEST_BADIO(RCP_sendHeartbeat);
    TEST_BADIO(RCP_sendSimpleActuatorWrite, 0, RCP_SIMPLE_ACTUATOR_TOGGLE);
    TEST_BADIO(RCP_sendStepperWrite, 0, RCP_STEPPER_SPEED_CONTROL, 0);
    TEST_BADIO(RCP_requestAngledActuatorWrite, 0, 0);
    TEST_BADIO(RCP_requestGeneralRead, RCP_DEVCLASS_TEST_STATE, 0);
    TEST_BADIO(RCP_requestTareConfiguration, RCP_DEVCLASS_GYROSCOPE, 0, 0, 0);
    TEST_BADIO(RCP_promptRespondGONOGO, RCP_GONOGO_GO);
    TEST_BADIO(RCP_promptRespondFloat, 0);

    uint8_t val = 3;
    TEST_BADIO(RCP_sendRawSerial, &val, 1);
}

// ------------ SECTION: Testing special return values -3 and 1 ------------ //

TEST_F(RCP, TareConfigurationSpecialReturn) {
    EXPECT_EQ(RCP_requestTareConfiguration(RCP_DEVCLASS_TEST_STATE, 0, 0, 0), -3);
}

TEST_F(RCP, SendRawSerialSpecialReturn) {
    EXPECT_EQ(RCP_sendRawSerial(nullptr, 100), -3) << "Incorrect return on too large of a raw packet";
    EXPECT_EQ(RCP_sendRawSerial(nullptr, 0), -3) << "Incorrect return on zero length raw packet";
}

// ------------ SECTION: Basic RCP tests specific to this library implementation ------------ //

// Test constants match those defined in the spec
TEST(RCPConstants, Constants) {
    // Channels
    EXPECT_EQ(RCP_CH_ZERO, 0x00);
    EXPECT_EQ(RCP_CH_ONE, 0x40);
    EXPECT_EQ(RCP_CH_TWO, 0x80);
    EXPECT_EQ(RCP_CH_THREE, 0xC0);

    // Device classes
    EXPECT_EQ(RCP_DEVCLASS_TEST_STATE, 0x00);
    EXPECT_EQ(RCP_DEVCLASS_SIMPLE_ACTUATOR, 0x01);
    EXPECT_EQ(RCP_DEVCLASS_STEPPER, 0x02);
    EXPECT_EQ(RCP_DEVCLASS_PROMPT, 0x03);
    EXPECT_EQ(RCP_DEVCLASS_ANGLED_ACTUATOR, 0x04);
    EXPECT_EQ(RCP_DEVCLASS_CUSTOM, 0x80);
    EXPECT_EQ(RCP_DEVCLASS_AM_PRESSURE, 0x90);
    EXPECT_EQ(RCP_DEVCLASS_AM_TEMPERATURE, 0x91);
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

TEST_F(RCP, IsOpen) { ASSERT_TRUE(RCP_isOpen()) << "RCP Is Open"; }

TEST_F(RCP, SetChannel) {
    ASSERT_EQ(RCP_getChannel(), RCP_CH_ZERO) << "RCP Default channel is not zero";
    RCP_setChannel(RCP_CH_ONE);
    ASSERT_EQ(RCP_getChannel(), RCP_CH_ONE) << "RCP Channel not set";
}

// ------------ SECTION: Tests on the return values of RCP_poll under exceptional circumstances ------------ //

TEST_F(RCPIn, PollEmpty) { ASSERT_EQ(RCP_poll(), -2) << "RCP_poll on empty buffer does not early return"; }

TEST_F(RCPIn, PollWrongChannel) {
    PUSH(0x41);
    PUSH(0x00);
    PUSH(0x00);

    EXPECT_EQ(RCP_poll(), 1) << "RCP_poll on wrong channel returns incorrectly";
    EXPECT_TRUE(IN.isEmpty()) << "Remaining packet not read from inbuffer on wrong channel";
}

TEST_F(RCPIn, PollIncompletePacket) {
    PUSH(0x05);

    EXPECT_EQ(RCP_poll(), -2) << "RCP_poll on missing bytes after header returns incorrectly";
    EXPECT_TRUE(IN.isEmpty()) << "Inbuffer not empty on missing parameter/class bytes poll";

    PUSH(0x05);
    PUSH(0x00);
    PUSH(0x00);

    EXPECT_EQ(RCP_poll(), -2) << "RCP_poll on missing bytes after class returns incorrectly";
    EXPECT_TRUE(IN.isEmpty()) << "Inbuffer not empty on missing parameter/class bytes poll";
}

// ------------ SECTION: Tests for setting timestamp bytes in the correct order ------------ //

TEST_F(RCPTestState, TimestampHighByte) {
    PUSH(0x05);
    PUSH(0x00);
    PUSH(0xFF);
    PUSH(0x00);
    PUSH(0x00);
    PUSH(0x00);
    PUSH(0x00);

    RCP_poll();
    EXPECT_EQ(TESTDATA.timestamp, 0xFF000000) << "Timestamp high order byte failure";
}

TEST_F(RCPTestState, Timestamp3rdByte) {
    PUSH(0x05);
    PUSH(0x00);
    PUSH(0x00);
    PUSH(0xFF);
    PUSH(0x00);
    PUSH(0x00);
    PUSH(0x00);

    RCP_poll();
    EXPECT_EQ(TESTDATA.timestamp, 0x00FF0000) << "Timestamp 3rd order byte failure";
}

TEST_F(RCPTestState, Timestamp2ndByte) {
    PUSH(0x05);
    PUSH(0x00);
    PUSH(0x00);
    PUSH(0x00);
    PUSH(0xFF);
    PUSH(0x00);
    PUSH(0x00);

    RCP_poll();
    EXPECT_EQ(TESTDATA.timestamp, 0x0000FF00) << "Timestamp 2nd order byte failure";
}

TEST_F(RCPTestState, TimestampLowByte) {
    PUSH(0x05);
    PUSH(0x00);
    PUSH(0x00);
    PUSH(0x00);
    PUSH(0x00);
    PUSH(0xFF);
    PUSH(0x00);

    RCP_poll();
    EXPECT_EQ(TESTDATA.timestamp, 0x000000FF) << "Timestamp low order byte failure";
}

#define PUSH_TIMESTAMP()                                                                                               \
    do {                                                                                                               \
        PUSH(0x00);                                                                                                    \
        PUSH(0x00);                                                                                                    \
        PUSH(0x00);                                                                                                    \
        PUSH(0x00);                                                                                                    \
    }                                                                                                                  \
    while(0)

#define PUSH_TEST_HEADER()                                                                                             \
    do {                                                                                                               \
        PUSH(0x05);                                                                                                    \
        PUSH(RCP_DEVCLASS_TEST_STATE);                                                                                 \
        PUSH_TIMESTAMP();                                                                                              \
    }                                                                                                                  \
    while(0)

// ------------ SECTION: Testing reading the various test state fields ------------ //

TEST_F(RCPTestState, DatastreamingOff) {
    PUSH_TEST_HEADER();
    PUSH(0x00);

    RCP_poll();
    EXPECT_FALSE(TESTDATA.dataStreaming) << "Datastreaming on when set to false";
}

TEST_F(RCPTestState, DataStreamingOn) {
    PUSH_TEST_HEADER();
    PUSH(0x80);

    RCP_poll();
    EXPECT_TRUE(TESTDATA.dataStreaming) << "Datastreaming off when set to true";
}

#define TEST_TESTSTATE(rstate)                                                                                         \
    TEST_F(RCPTestState, RunningState_##rstate) {                                                                      \
        PUSH_TEST_HEADER();                                                                                            \
        PUSH(rstate);                                                                                                  \
        RCP_poll();                                                                                                    \
        EXPECT_EQ(TESTDATA.state, rstate) << #rstate " not set correctly";                                             \
    }

TEST_TESTSTATE(RCP_TEST_RUNNING);
TEST_TESTSTATE(RCP_TEST_STOPPED);
TEST_TESTSTATE(RCP_TEST_PAUSED);
TEST_TESTSTATE(RCP_TEST_ESTOP);

TEST_F(RCPTestState, TestStateInitOff) {
    PUSH_TEST_HEADER();
    PUSH(0x00);

    RCP_poll();
    EXPECT_FALSE(TESTDATA.isInited) << "Inited true when set false";
}

TEST_F(RCPTestState, TestStateInitOn) {
    PUSH_TEST_HEADER();
    PUSH(0x10);

    RCP_poll();
    EXPECT_TRUE(TESTDATA.isInited) << "Inited false when set true";
}

TEST_F(RCPTestState, TestStateZeroHeartbeats) {
    PUSH_TEST_HEADER();
    PUSH(0x00);

    RCP_poll();
    EXPECT_EQ(TESTDATA.heartbeatTime, 0) << "Heartbeat time not zero";
}

TEST_F(RCPTestState, TestStateMaxHeartbeats) {
    PUSH_TEST_HEADER();
    PUSH(0x0E);

    RCP_poll();
    EXPECT_EQ(TESTDATA.heartbeatTime, 14) << "Heartbeat time not 14";
}

// ------------ SECTION: Testing the simple actuator fields ------------ //

TEST_F(RCPSactD, SimpleActuatorID) {
    PUSH(0x06);
    PUSH(RCP_DEVCLASS_SIMPLE_ACTUATOR);
    PUSH_TIMESTAMP();
    PUSH(0x55);
    PUSH(0x00);

    RCP_poll();
    EXPECT_EQ(SACTD.ID, 0x55);
}

TEST_F(RCPSactD, ActOff) {
    PUSH(0x06);
    PUSH(RCP_DEVCLASS_SIMPLE_ACTUATOR);
    PUSH_TIMESTAMP();
    PUSH(0x00);
    PUSH(0x00);

    RCP_poll();
    EXPECT_EQ(SACTD.state, RCP_SIMPLE_ACTUATOR_OFF);
}

TEST_F(RCPSactD, ActOn) {
    PUSH(0x06);
    PUSH(RCP_DEVCLASS_SIMPLE_ACTUATOR);
    PUSH_TIMESTAMP();
    PUSH(0x00);
    PUSH(0x80);

    RCP_poll();
    EXPECT_EQ(SACTD.state, RCP_SIMPLE_ACTUATOR_ON);
}

// ------------ SECTION: BoolSensor fields ------------ //

TEST_F(RCPBoolData, BoolSenseID) {
    PUSH(0x06);
    PUSH(RCP_DEVCLASS_BOOL_SENSOR);
    PUSH_TIMESTAMP();
    PUSH(0x55);
    PUSH(0x00);

    RCP_poll();
    EXPECT_EQ(BOOLDATA.ID, 0x55);
}

TEST_F(RCPBoolData, BoolSenseOff) {
    PUSH(0x06);
    PUSH(RCP_DEVCLASS_BOOL_SENSOR);
    PUSH_TIMESTAMP();
    PUSH(0x00);
    PUSH(0x00);

    RCP_poll();
    EXPECT_FALSE(BOOLDATA.data);
}

TEST_F(RCPBoolData, BoolSenseOn) {
    PUSH(0x06);
    PUSH(RCP_DEVCLASS_BOOL_SENSOR);
    PUSH_TIMESTAMP();
    PUSH(0x00);
    PUSH(0x80);

    RCP_poll();
    EXPECT_TRUE(BOOLDATA.data);
}

// ------------ SECTION: Receiving custom data ------------ //

TEST_F(RCPCustomData, SingleByte) {
    PUSH(0x01);
    PUSH(RCP_DEVCLASS_CUSTOM);
    PUSH(0x05);

    RCP_poll();
    EXPECT_EQ(CDATA.length, 1) << "Custom data length incorrect";
    EXPECT_EQ(static_cast<const uint8_t*>(CDATA.data)[0], 0x05) << "Custom data byte 0 incorrect";
}

TEST_F(RCPCustomData, Multibyte) {
    PUSH(0x05);
    PUSH(RCP_DEVCLASS_CUSTOM);
    PUSH(0x10);
    PUSH(0x01);
    PUSH(0x02);
    PUSH(0x03);
    PUSH(0x04);

    RCP_poll();
    EXPECT_EQ(CDATA.length, 5) << "Custom data length incorrect";
    EXPECT_EQ(static_cast<const uint8_t*>(CDATA.data)[0], 0x10) << "Custom data byte 0 incorrect";
    EXPECT_EQ(static_cast<const uint8_t*>(CDATA.data)[4], 0x04) << "Custom data byte 4 incorrect";
}

#define PUSHHEXFLOAT(val)                                                                                              \
    do {                                                                                                               \
        PUSH(val >> 24);                                                                                               \
        PUSH(val >> 16);                                                                                               \
        PUSH(val >> 8);                                                                                                \
        PUSH(val);                                                                                                     \
    }                                                                                                                  \
    while(0)

// ------------ SECTION: Floating point returns ------------ //

TEST_F(RCPFloats, Float1ID) {
    PUSH(0x09);
    PUSH(RCP_DEVCLASS_AM_PRESSURE);
    PUSH_TIMESTAMP();
    PUSH(0x55);
    PUSHHEXFLOAT(HPI);

    RCP_poll();
    EXPECT_EQ(FLOATS->float1.ID, 0x55);
}

TEST_F(RCPFloats, Float1) {
    PUSH(0x09);
    PUSH(RCP_DEVCLASS_AM_PRESSURE);
    PUSH_TIMESTAMP();
    PUSH(0x00);
    PUSHHEXFLOAT(HPI);

    RCP_poll();
    EXPECT_EQ(FLOATS->float1.data, PI);
}

TEST_F(RCPFloats, Float2ID) {
    PUSH(0x0D);
    PUSH(RCP_DEVCLASS_POWERMON);
    PUSH_TIMESTAMP();
    PUSH(0x55);
    PUSHHEXFLOAT(HPI);
    PUSHHEXFLOAT(HPI);

    RCP_poll();
    EXPECT_EQ(FLOATS->float2.ID, 0x55);
}

TEST_F(RCPFloats, Float2) {
    PUSH(0x0D);
    PUSH(RCP_DEVCLASS_POWERMON);
    PUSH_TIMESTAMP();
    PUSH(0x00);
    PUSHHEXFLOAT(HPI);
    PUSHHEXFLOAT(HPI2);

    RCP_poll();
    EXPECT_EQ(FLOATS->float2.data[0], PI);
    EXPECT_EQ(FLOATS->float2.data[1], PI2);
}

TEST_F(RCPFloats, Float3ID) {
    PUSH(0x11);
    PUSH(RCP_DEVCLASS_ACCELEROMETER);
    PUSH_TIMESTAMP();
    PUSH(0x55);
    PUSHHEXFLOAT(HPI);
    PUSHHEXFLOAT(HPI);
    PUSHHEXFLOAT(HPI);

    RCP_poll();
    EXPECT_EQ(FLOATS->float3.ID, 0x55);
}

TEST_F(RCPFloats, Float3) {
    PUSH(0x11);
    PUSH(RCP_DEVCLASS_ACCELEROMETER);
    PUSH_TIMESTAMP();
    PUSH(0x00);
    PUSHHEXFLOAT(HPI);
    PUSHHEXFLOAT(HPI2);
    PUSHHEXFLOAT(HPI3);

    RCP_poll();
    EXPECT_EQ(FLOATS->float3.data[0], PI);
    EXPECT_EQ(FLOATS->float3.data[1], PI2);
    EXPECT_EQ(FLOATS->float3.data[2], PI3);
}

TEST_F(RCPFloats, Float4ID) {
    PUSH(0x015);
    PUSH(RCP_DEVCLASS_GPS);
    PUSH_TIMESTAMP();
    PUSH(0x55);
    PUSHHEXFLOAT(HPI);
    PUSHHEXFLOAT(HPI);
    PUSHHEXFLOAT(HPI);
    PUSHHEXFLOAT(HPI);

    RCP_poll();
    EXPECT_EQ(FLOATS->float4.ID, 0x55);
}

TEST_F(RCPFloats, Float4) {
    PUSH(0x15);
    PUSH(RCP_DEVCLASS_GPS);
    PUSH_TIMESTAMP();
    PUSH(0x00);
    PUSHHEXFLOAT(HPI);
    PUSHHEXFLOAT(HPI2);
    PUSHHEXFLOAT(HPI3);
    PUSHHEXFLOAT(HPI4);

    RCP_poll();
    EXPECT_EQ(FLOATS->float4.data[0], PI);
    EXPECT_EQ(FLOATS->float4.data[1], PI2);
    EXPECT_EQ(FLOATS->float4.data[2], PI3);
    EXPECT_EQ(FLOATS->float4.data[3], PI4);
}

// ------------ SECTION: Receiving prompt input requests ------------ //

TEST_F(RCPPromptInputRequest, PIRCLEAR) {
    PUSH(0x01);
    PUSH(RCP_DEVCLASS_PROMPT);
    PUSH(RCP_PromptDataType_RESET);

    RCP_poll();
    EXPECT_EQ(PIR.type, RCP_PromptDataType_RESET) << "Incorrect reset prompt type";
    EXPECT_EQ(PIR.prompt, nullptr) << "Incorrect reset prompt string";
}

TEST_F(RCPPromptInputRequest, PIRGNG) {
    PUSH(0x06);
    PUSH(RCP_DEVCLASS_PROMPT);
    PUSH(RCP_PromptDataType_GONOGO);
    PUSHHELLO();

    RCP_poll();
    EXPECT_EQ(PIR.type, RCP_PromptDataType_GONOGO) << "Incorrect GNG prompt type";
    EXPECT_STREQ(PIR.prompt, HELLO) << "Prompt string mismatch";
}

TEST_F(RCPPromptInputRequest, PIRFloat) {
    PUSH(0x06);
    PUSH(RCP_DEVCLASS_PROMPT);
    PUSH(RCP_PromptDataType_Float);
    PUSHHELLO();

    RCP_poll();
    EXPECT_EQ(PIR.type, RCP_PromptDataType_Float) << "Incorrect float prompt type";
    EXPECT_STREQ(PIR.prompt, HELLO) << "Prompt string mismatch";
}

#define CHECK_OUTBUF(...)                                                                                              \
    do {                                                                                                               \
        uint8_t vals[] = {__VA_ARGS__};                                                                                \
        EXPECT_EQ(OUT.size(), sizeof(vals)) << "Incorrect size of outbuffer";                                          \
        for(int i = 0; i < sizeof(vals); i++) {                                                                        \
            uint8_t val = POP();                                                                                       \
            EXPECT_EQ(val, vals[i]) << "Incorrect value at index " << i;                                               \
        }                                                                                                              \
    }                                                                                                                  \
    while(0)

// ------------ SECTION: RCP Outputs ------------ //

TEST_F(RCPOut, ESTOP) {
    EXPECT_EQ(RCP_sendEStop(), 0);
    CHECK_OUTBUF(0x00);
}

TEST_F(RCPOut, TestBegin) {
    EXPECT_EQ(RCP_startTest(3), 0);
    CHECK_OUTBUF(0x01, 0x00, 0x03);
}

TEST_F(RCPOut, TestStop) {
    EXPECT_EQ(RCP_stopTest(), 0);
    CHECK_OUTBUF(0x01, 0x00, 0x10);
}

TEST_F(RCPOut, TestPauseUnpause) {
    EXPECT_EQ(RCP_pauseUnpauseTest(), 0);
    CHECK_OUTBUF(0x01, 0x00, 0x11);
}

TEST_F(RCPOut, DeviceReset) {
    EXPECT_EQ(RCP_deviceReset(), 0);
    CHECK_OUTBUF(0x01, 0x00, 0x12);
}

TEST_F(RCPOut, TimebaseReset) {
    EXPECT_EQ(RCP_deviceTimeReset(), 0);
    CHECK_OUTBUF(0x01, 0x00, 0x13);
}

TEST_F(RCPOut, DataStreamingStop) {
    EXPECT_EQ(RCP_setDataStreaming(false), 0);
    CHECK_OUTBUF(0x01, 0x00, 0x20);
}

TEST_F(RCPOut, DataStreamingStart) {
    EXPECT_EQ(RCP_setDataStreaming(true), 0);
    CHECK_OUTBUF(0x01, 0x00, 0x21);
}

TEST_F(RCPOut, TestQuery) {
    EXPECT_EQ(RCP_requestTestState(), 0);
    CHECK_OUTBUF(0x01, 0x00, 0x30);
}

TEST_F(RCPOut, DisableHeartbeat) {
    EXPECT_EQ(RCP_setHeartbeatTime(0), 0);
    CHECK_OUTBUF(0x01, 0x00, 0xF0);
}

TEST_F(RCPOut, EnableHeartbeat) {
    EXPECT_EQ(RCP_setHeartbeatTime(5), 0);
    CHECK_OUTBUF(0x01, 0x00, 0xF5);
}

TEST_F(RCPOut, SendHeartbeat) {
    EXPECT_EQ(RCP_sendHeartbeat(), 0);
    CHECK_OUTBUF(0x01, 0x00, 0xFF);
}

TEST_F(RCPOut, SimpleActuatorOff) {
    EXPECT_EQ(RCP_sendSimpleActuatorWrite(5, RCP_SIMPLE_ACTUATOR_OFF), 0);
    CHECK_OUTBUF(0x02, RCP_DEVCLASS_SIMPLE_ACTUATOR, 0x05, RCP_SIMPLE_ACTUATOR_OFF);
}

TEST_F(RCPOut, SimpleActuatorOn) {
    EXPECT_EQ(RCP_sendSimpleActuatorWrite(5, RCP_SIMPLE_ACTUATOR_ON), 0);
    CHECK_OUTBUF(0x02, RCP_DEVCLASS_SIMPLE_ACTUATOR, 0x05, RCP_SIMPLE_ACTUATOR_ON);
}

TEST_F(RCPOut, SimpleActuatorToggle) {
    EXPECT_EQ(RCP_sendSimpleActuatorWrite(5, RCP_SIMPLE_ACTUATOR_TOGGLE), 0);
    CHECK_OUTBUF(0x02, RCP_DEVCLASS_SIMPLE_ACTUATOR, 0x05, RCP_SIMPLE_ACTUATOR_TOGGLE);
}

TEST_F(RCPOut, StepperAbsolutePositioning) {
    EXPECT_EQ(RCP_sendStepperWrite(5, RCP_STEPPER_ABSOLUTE_POS_CONTROL, PI), 0);
    CHECK_OUTBUF(0x06, RCP_DEVCLASS_STEPPER, 0x05, RCP_STEPPER_ABSOLUTE_POS_CONTROL, HFLOATARR(HPI));
}

TEST_F(RCPOut, StepperRelativePositioning) {
    EXPECT_EQ(RCP_sendStepperWrite(5, RCP_STEPPER_RELATIVE_POS_CONTROL, PI), 0);
    CHECK_OUTBUF(0x06, RCP_DEVCLASS_STEPPER, 0x05, RCP_STEPPER_RELATIVE_POS_CONTROL, HFLOATARR(HPI));
}

TEST_F(RCPOut, StepperSpeedControl) {
    EXPECT_EQ(RCP_sendStepperWrite(5, RCP_STEPPER_SPEED_CONTROL, PI), 0);
    CHECK_OUTBUF(0x06, RCP_DEVCLASS_STEPPER, 0x05, RCP_STEPPER_SPEED_CONTROL, HFLOATARR(HPI));
}

TEST_F(RCPOut, PromptResponseGNG_GO) {
    EXPECT_EQ(RCP_promptRespondGONOGO(RCP_GONOGO_GO), 0);
    CHECK_OUTBUF(0x01, RCP_DEVCLASS_PROMPT, RCP_GONOGO_GO);
}

TEST_F(RCPOut, PromptResponseGNG_NOGO) {
    EXPECT_EQ(RCP_promptRespondGONOGO(RCP_GONOGO_NOGO), 0);
    CHECK_OUTBUF(0x01, RCP_DEVCLASS_PROMPT, RCP_GONOGO_NOGO);
}

TEST_F(RCPOut, PromptResponseFloat) {
    EXPECT_EQ(RCP_promptRespondFloat(PI), 0);
    CHECK_OUTBUF(0x04, RCP_DEVCLASS_PROMPT, HFLOATARR(HPI));
}

TEST_F(RCPOut, AngledActuatorWrite) {
    EXPECT_EQ(RCP_requestAngledActuatorWrite(5, PI), 0);
    CHECK_OUTBUF(0x05, RCP_DEVCLASS_ANGLED_ACTUATOR, 0x05, HFLOATARR(HPI));
}

TEST_F(RCPOut, SensorReadRequest) {
    EXPECT_EQ(RCP_requestGeneralRead(RCP_DEVCLASS_AM_PRESSURE, 0x05), 0);
    CHECK_OUTBUF(0x01, RCP_DEVCLASS_AM_PRESSURE, 0x05);
}

TEST_F(RCPOut, SensorTareRequest) {
    EXPECT_EQ(RCP_requestTareConfiguration(RCP_DEVCLASS_POWERMON, 5, 1, PI), 0);
    CHECK_OUTBUF(0x06, RCP_DEVCLASS_POWERMON, 0x05, 0x01, HFLOATARR(HPI));
}

TEST_F(RCPOut, SendRawSerial) {
    uint8_t vals[] = {HELLOHEX};
    EXPECT_EQ(RCP_sendRawSerial(vals, sizeof(vals)), 0);
    uint8_t len = sizeof(vals);
    CHECK_OUTBUF(len, RCP_DEVCLASS_CUSTOM, HELLOHEX);
}
