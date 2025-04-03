#ifndef RCP_HOST_H
#define RCP_HOST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RCP_CH_ZERO = 0x00,
    RCP_CH_ONE = 0x40,
    RCP_CH_TWO = 0x80,
    RCP_CH_THREE = 0xC0,
    RCP_CHANNEL_MASK = 0xC0,
} RCP_Channel;

typedef enum {
    RCP_DEVCLASS_TEST_STATE = 0x00,
    RCP_DEVCLASS_SOLENOID = 0x01,
    RCP_DEVCLASS_STEPPER = 0x02,
    RCP_DEVCLASS_PROMPT = 0x03,
    RCP_DEVCLASS_CUSTOM = 0x80,

    RCP_DEVCLASS_AM_PRESSURE = 0x90,
    RCP_DEVCLASS_AM_TEMPERATURE = 0x91,
    RCP_DEVCLASS_PRESSURE_TRANSDUCER = 0x92,
    RCP_DEVCLASS_RELATIVE_HYGROMETER = 0x93,
    RCP_DEVCLASS_LOAD_CELL = 0x94,

    RCP_DEVCLASS_POWERMON = 0xA0,

    RCP_DEVCLASS_ACCELEROMETER = 0xB0,
    RCP_DEVCLASS_GYROSCOPE = 0xB1,
    RCP_DEVCLASS_MAGNETOMETER = 0xB2,

    RCP_DEVCLASS_GPS = 0xC0,
} RCP_DeviceClass;

typedef enum {
    RCP_TEST_START = 0x00,
    RCP_TEST_STOP = 0x10,
    RCP_TEST_PAUSE = 0x11,
    RCP_DEVICE_RESET = 0x12,
    RCP_DATA_STREAM_STOP = 0x20,
    RCP_DATA_STREAM_START = 0x21,
    RCP_TEST_QUERY = 0x30,
    RCP_HEARTBEATS_CONTROL = 0xF0
} RCP_TestStateControlMode;


typedef enum {
    RCP_TEST_RUNNING = 0x00,
    RCP_DEVICE_INITED_MASK = 0x10,
    RCP_TEST_STOPPED = 0x20,
    RCP_TEST_PAUSED = 0x40,
    RCP_TEST_ESTOP = 0x60,
    RCP_TEST_STATE_MASK = 0x60,
} RCP_TestRunningState;

typedef enum {
    RCP_SOLENOID_READ = 0x00,
    RCP_SOLENOID_ON = 0x40,
    RCP_SOLENOID_OFF = 0x80,
    RCP_SOLENOID_TOGGLE = 0xC0,
    RCP_SOLENOID_STATE_MASK = 0xC0,
} RCP_SolenoidState;

typedef enum {
    RCP_STEPPER_QUERY_STATE = 0x00,
    RCP_STEPPER_ABSOLUTE_POS_CONTROL = 0x40,
    RCP_STEPPER_RELATIVE_POS_CONTROL = 0x80,
    RCP_STEPPER_SPEED_CONTROL = 0xC0,
    RCP_STEPPER_CONTROL_MODE_MASK = 0xC0
} RCP_StepperControlMode;

typedef enum {
    RCP_PromptDataType_GONOGO = 0x00,
    RCP_PromptDataType_Float = 0x01,
} RCP_PromptDataType;

typedef enum {
    RCP_GONOGO_NOGO = 0x00,
    RCP_GONOGO_GO = 0x01,
} RCP_GONOGO;

struct RCP_TestData {
    uint32_t timestamp;
    int dataStreaming;
    RCP_TestRunningState state;
    int isInited;
    uint8_t heartbeatTime;
};

struct RCP_SolenoidData {
    uint32_t timestamp;
    RCP_SolenoidState state;
    uint8_t ID;
};

struct RCP_PromptInputRequest {
    const RCP_PromptDataType type;
    char* const prompt;
};

struct RCP_OneFloat {
    RCP_DeviceClass devclass;
    uint32_t timestamp;
    uint8_t ID;
    float data;
};

struct RCP_TwoFloat {
    RCP_DeviceClass devclass;
    uint32_t timestamp;
    uint8_t ID;
    float data[2];
};

struct RCP_ThreeFloat {
    RCP_DeviceClass devclass;
    uint32_t timestamp;
    uint8_t ID;
    float data[3];
};

struct RCP_FourFloat {
    RCP_DeviceClass devclass;
    uint32_t timestamp;
    uint8_t ID;
    float data[4];
};

struct RCP_CustomData {
    void* data;
    uint8_t length;
};

struct RCP_LibInitData {
    size_t (*sendData)(const void* data, size_t length);
    size_t (*readData)(void* data, size_t length);
    int (*processTestUpdate)(struct RCP_TestData data);
    int (*processSolenoidData)(struct RCP_SolenoidData data);
    int (*processPromptInput)(struct RCP_PromptInputRequest request);
    int (*processSerialData)(struct RCP_CustomData data);
    int (*processOneFloat)(struct RCP_OneFloat data);
    int (*processTwoFloat)(struct RCP_TwoFloat data);
    int (*processThreeFloat)(struct RCP_ThreeFloat data);
    int (*processFourFloat)(struct RCP_FourFloat data);
};

// Provide library with callbacks to needed functions
int RCP_init(struct RCP_LibInitData callbacks);
int RCP_isOpen();
int RCP_shutdown();

// Library will default to channel zero, but it can be changed here.
void RCP_setChannel(RCP_Channel ch);

// Function to call periodically to poll for data
int RCP_poll();

// Functions to send controller packets
int RCP_sendEStop();
int RCP_sendHeartbeat();

int RCP_startTest(uint8_t testnum);
int RCP_stopTest();
int RCP_pauseUnpauseTest();
int RCP_deviceReset();
int RCP_setDataStreaming(int datastreaming);
int RCP_changeTestProgress(RCP_TestStateControlMode mode);
int RCP_setHeartbeatTime(uint8_t heartbeatTime);
int RCP_requestTestState();

int RCP_sendSolenoidWrite(uint8_t ID, RCP_SolenoidState state);
int RCP_requestSolenoidRead(uint8_t ID);

// Value should be a 4 byte integral type
int RCP_sendStepperWrite(uint8_t ID, RCP_StepperControlMode mode, const void* value);
int RCP_requestStepperRead(uint8_t ID);

int RCP_requestDeviceReadNOID(RCP_DeviceClass device);
int RCP_requestDeviceReadID(RCP_DeviceClass device, uint8_t ID);

int RCP_promptRespondGONOGO(RCP_GONOGO gonogo);
int RCP_promptRespondFloat(float value);

int RCP_sendRawSerial(const uint8_t* data, uint8_t size);

#ifdef __cplusplus
}
#endif

#endif //RCP_HOST_H
