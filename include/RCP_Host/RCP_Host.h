#ifndef RCP_HOST_H
#define RCP_HOST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const char* const RCP_VERSION;
extern const char* const RCP_VERSION_END;

#define RCP_MAX_COMPACT_BYTES 63
#define RCP_MAX_NON_PARAM 4
#define RCP_MAX_EXTENDED_BYTES 65536

typedef enum {
    RCP_ERR_SUCCESS = 0,
    RCP_ERR_INIT = 1,
    RCP_ERR_MEMALLOC = 2,
    RCP_ERR_IO_SEND = 3,
    RCP_ERR_INVALID_DEVCLASS = 4,
    RCP_ERR_NO_ACTIVE_PROMPT = 5,
    RCP_ERR_IO_RCV = 6,
    RCP_ERR_AMALG_NESTING = 7,
    RCP_ERR_AMALG_SUBUNIT = 8,
} RCP_Error;

#define RCP_EXTENDED_MASK 0x40
#define RCP_COMPACT_LENGTH_MASK 0x3F

typedef enum {
    RCP_CH_ZERO = 0x00,
    RCP_CH_ONE = 0x80,
    RCP_CHANNEL_MASK = 0xC0,
} RCP_Channel;

typedef enum {
    RCP_DEVCLASS_TEST_STATE = 0x00,
    RCP_DEVCLASS_SIMPLE_ACTUATOR = 0x01,
    RCP_DEVCLASS_STEPPER = 0x02,
    RCP_DEVCLASS_PROMPT = 0x03,
    RCP_DEVCLASS_ANGLED_ACTUATOR = 0x04,
    RCP_DEVCLASS_TARGET_LOG = 0x80,

    RCP_DEVCLASS_AM_PRESSURE = 0x90,
    RCP_DEVCLASS_TEMPERATURE = 0x91,
    RCP_DEVCLASS_PRESSURE_TRANSDUCER = 0x92,
    RCP_DEVCLASS_RELATIVE_HYGROMETER = 0x93,
    RCP_DEVCLASS_LOAD_CELL = 0x94,
    RCP_DEVCLASS_BOOL_SENSOR = 0x95,

    RCP_DEVCLASS_POWERMON = 0xA0,

    RCP_DEVCLASS_ACCELEROMETER = 0xB0,
    RCP_DEVCLASS_GYROSCOPE = 0xB1,
    RCP_DEVCLASS_MAGNETOMETER = 0xB2,

    RCP_DEVCLASS_GPS = 0xC0,

    RCP_DEVCLASS_AMALGAMATE = 0xFF
} RCP_DeviceClass;

typedef enum {
    RCP_TEST_START = 0x00,
    RCP_TEST_STOP = 0x10,
    RCP_TEST_PAUSE = 0x11,
    RCP_DEVICE_RESET = 0x12,
    RCP_DEVICE_RESET_TIME = 0x13,
    RCP_DATA_STREAM_STOP = 0x20,
    RCP_DATA_STREAM_START = 0x21,
    RCP_TEST_QUERY = 0x30,
    RCP_HEARTBEATS_CONTROL = 0xF0,
    RCP_HEARTBEAT = 0xFF
} RCP_TestStateControlMode;

typedef enum {
    RCP_TEST_RUNNING = 0x00,
    RCP_DEVICE_INITED_MASK = 0x10,
    RCP_TEST_STOPPED = 0x20,
    RCP_TEST_PAUSED = 0x40,
    RCP_TEST_ESTOP = 0x60,
    RCP_TEST_STATE_MASK = 0x60,
    RCP_DATA_STREAM_MASK = 0x80,
    RCP_HEARTBEAT_TIME_MASK = 0x0F,
} RCP_TestRunningState;

typedef enum {
    RCP_SIMPLE_ACTUATOR_OFF = 0x00,
    RCP_SIMPLE_ACTUATOR_ON = 0x80,
    RCP_SIMPLE_ACTUATOR_TOGGLE = 0xC0,
} RCP_SimpleActuatorState;

typedef enum {
    RCP_STEPPER_ABSOLUTE_POS_CONTROL = 0x40,
    RCP_STEPPER_RELATIVE_POS_CONTROL = 0x80,
    RCP_STEPPER_SPEED_CONTROL = 0xC0,
    RCP_STEPPER_CONTROL_MASK = 0xC0,
} RCP_StepperControlMode;

typedef enum {
    RCP_PromptDataType_GONOGO = 0x00,
    RCP_PromptDataType_Float = 0x01,
    RCP_PromptDataType_RESET = 0xFF,
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
    uint8_t runningTest;
    uint8_t testProgress;
};

struct RCP_SimpleActuatorData {
    uint32_t timestamp;
    uint8_t ID;
    RCP_SimpleActuatorState state;
};

struct RCP_PromptInputRequest {
    RCP_PromptDataType type;
    const char* prompt;
    uint16_t length;
};

struct RCP_BoolData {
    uint32_t timestamp;
    uint8_t ID;
    int data;
};

struct RCP_1F {
    RCP_DeviceClass devclass;
    uint32_t timestamp;
    uint8_t ID;
    float data;
};

struct RCP_2F {
    RCP_DeviceClass devclass;
    uint32_t timestamp;
    uint8_t ID;
    float data[2];
};

struct RCP_3F {
    RCP_DeviceClass devclass;
    uint32_t timestamp;
    uint8_t ID;
    float data[3];
};

struct RCP_4F {
    RCP_DeviceClass devclass;
    uint32_t timestamp;
    uint8_t ID;
    float data[4];
};

struct RCP_TargetLogData {
    uint32_t timestamp;
    const char* data;
    uint16_t length;
};

struct RCP_LibInitData {
    size_t (*sendData)(const void* data, size_t length);
    size_t (*readData)(void* data, size_t length);
    RCP_Error (*processTestUpdate)(struct RCP_TestData data);
    RCP_Error (*processBoolData)(struct RCP_BoolData data);
    RCP_Error (*processSimpleActuatorData)(struct RCP_SimpleActuatorData data);
    RCP_Error (*processPromptInput)(struct RCP_PromptInputRequest request);
    RCP_Error (*processTargetLog)(struct RCP_TargetLogData data);
    RCP_Error (*processOneFloat)(struct RCP_1F data);
    RCP_Error (*processTwoFloat)(struct RCP_2F data);
    RCP_Error (*processThreeFloat)(struct RCP_3F data);
    RCP_Error (*processFourFloat)(struct RCP_4F data);
    void (*heartbeatReceived)();
};

// Provide library with callbacks to needed functions
RCP_Error RCP_init(struct RCP_LibInitData callbacks);
int RCP_isOpen(void);
RCP_Error RCP_shutdown(void);
const char* RCP_errstr(RCP_Error rerrno);

// Library will default to channel zero, but it can be changed here.
void RCP_setChannel(RCP_Channel ch);
RCP_Channel RCP_getChannel(void);

// Function to call periodically to poll for data
RCP_Error RCP_poll(void);

// Functions to send controller packets
RCP_Error RCP_sendEStop(void);
RCP_Error RCP_sendHeartbeat(void);

RCP_Error RCP_startTest(uint8_t testnum);
RCP_Error RCP_stopTest(void);
RCP_Error RCP_pauseUnpauseTest(void);
RCP_Error RCP_deviceReset(void);
RCP_Error RCP_deviceTimeReset(void);
RCP_Error RCP_setDataStreaming(int datastreaming);
RCP_Error RCP_setHeartbeatTime(uint8_t heartbeatTime);
RCP_Error RCP_requestTestState(void);

RCP_Error RCP_sendSimpleActuatorWrite(uint8_t ID, RCP_SimpleActuatorState state);
RCP_Error RCP_sendStepperWrite(uint8_t ID, RCP_StepperControlMode mode, float value);
RCP_Error RCP_sendAngledActuatorWrite(uint8_t ID, float value);

RCP_Error RCP_requestGeneralRead(RCP_DeviceClass device, uint8_t ID);
RCP_Error RCP_requestTareConfiguration(RCP_DeviceClass device, uint8_t ID, uint8_t dataChannel, float offset);

RCP_Error RCP_promptRespondGONOGO(RCP_GONOGO gonogo);
RCP_Error RCP_promptRespondFloat(float value);
RCP_PromptDataType RCP_getActivePromptType(void);

#ifdef __cplusplus
}
#endif

#endif // RCP_HOST_H
