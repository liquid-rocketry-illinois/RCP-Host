#ifndef FIXTURES_H
#define FIXTURES_H

#include "RCP_Host/RCP_Host.h"
#include "RingBuffer.h"
#include "gtest/gtest.h"

extern RCP_LibInitData initData;

class RCP;
extern RCP* context;

class RCP : public testing::Test {
protected:
    ~RCP() override = default;

    void SetUp() override {
        RCP_init(initData);
        RCP_setChannel(RCP_CH_ZERO);
        context = this;
    }

    void TearDown() override {
        RCP_shutdown();
        context = nullptr;
    }
};

class RCPIn : public RCP {
public:
    ~RCPIn() override = default;

    LRI::RCI::RingBuffer<uint8_t> inbuffer{128};
};

class RCPOut : public RCP {
public:
    ~RCPOut() override = default;

    LRI::RCI::RingBuffer<uint8_t> outbuffer{128};
};

class RCPTestState : public RCPIn {
public:
    ~RCPTestState() override = default;

    RCP_TestData testData;
};

class RCPBoolData : public RCPIn {
public:
    ~RCPBoolData() override = default;

    RCP_BoolData bdata;
};

class RCPSactD : public RCPIn {
public:
    ~RCPSactD() override = default;

    RCP_SimpleActuatorData sactd;
};

class RCPPromptInputRequest : public RCPIn {
public:
    ~RCPPromptInputRequest() override = default;

    RCP_PromptDataType type;
    char prompt[64];
};

class RCPCustomData : public RCPIn {
public:
    ~RCPCustomData() override = default;

    uint8_t length;
    uint8_t data[65];
};

class RCPFloats : public RCPIn {
public:
    ~RCPFloats() override = default;

    RCP_OneFloat float1{};
    RCP_TwoFloat float2{};
    RCP_ThreeFloat float3{};
    RCP_FourFloat float4{};
};

#endif // FIXTURES_H
