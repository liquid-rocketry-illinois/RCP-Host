#ifndef RCP_HOST_H
#define RCP_HOST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t RCP_Channel_t;

enum RCP_Channel {
    RCP_CH_ZERO = 0x00,
    RCP_CH_ONE = 0x40,
    RCP_CH_TWO = 0x80,
    RCP_CH_THREE = 0xC0,
    RCP_CHANNEL_MASK = 0xC0,
};

typedef uint8_t RCP_DeviceClass_t;

enum RCP_DeviceClass {
    RCP_DEVCLASS_TEST_STATE = 0x00,
    RCP_DEVCLASS_SOLENOID = 0x01,
    RCP_DEVCLASS_STEPPER = 0x02,
    RCP_DEVCLASS_CUSTOM = 0x80,
    RCP_DEVCLASS_GPS = 0x81,
    RCP_DEVCLASS_AM_PRESSURE = 0x82,
    RCP_DEVCLASS_AM_TEMPERATURE = 0x83,
    RCP_DEVCLASS_ACCELEROMETER = 0x84,
    RCP_DEVCLASS_GYROSCOPE = 0x85,
    RCP_DEVCLASS_MAGNETOMETER = 0x86,
    RCP_DEVCLASS_PRESSURE_TRANSDUCER = 0x87,
    RCP_DEVCLASS_RELATIVE_HYGROMETER = 0x88,
};

typedef uint8_t RCP_TestStateControlMode_t;

enum RCP_TestStateControlMode {
    RCP_TEST_START = 0x00,
    RCP_TEST_STOP = 0x10,
    RCP_TEST_PAUSE = 0x11,
    RCP_DATA_STREAM_STOP = 0x20,
    RCP_DATA_STREAM_START = 0x21,
    RCP_TEST_QUERY = 0x30,
    RCP_HEARTBEATS_CONTROL = 0xF0
};

typedef uint8_t RCP_TestRunningState_t;

enum RCP_TestRunningState {
    RCP_TEST_RUNNING = 0x00,
    RCP_TEST_STOPPED = 0x20,
    RCP_TEST_PAUSED = 0x40,
    RCP_TEST_ESTOP = 0x60,
    RCP_TEST_STATE_MASK = 0x60,
};

typedef uint8_t RCP_SolenoidState_t;

enum RCP_SolenoidState {
    RCP_SOLENOID_READ = 0x00,
    RCP_SOLENOID_ON = 0x40,
    RCP_SOLENOID_OFF = 0x80,
    RCP_SOLENOID_TOGGLE = 0xC0,
    RCP_SOLENOID_STATE_MASK = 0xC0,
};

typedef uint8_t RCP_StepperControlMode_t;

enum RCP_StepperControlMode {
    RCP_STEPPER_QUERY_STATE = 0x00,
    RCP_STEPPER_ABSOLUTE_POS_CONTROL = 0x40,
    RCP_STEPPER_RELATIVE_POS_CONTROL = 0x80,
    RCP_STEPPER_SPEED_CONTROL = 0xC0,
    RCP_STEPPER_CONTROL_MODE_MASK = 0xC0
};

struct RCP_TestData {
    uint32_t timestamp;
    int dataStreaming;
    RCP_TestRunningState_t state;
    uint8_t heartbeatTime;
};

struct RCP_SolenoidData {
    uint32_t timestamp;
    RCP_SolenoidState_t state;
    uint8_t ID;
};

struct RCP_StepperData {
    uint32_t timestamp;
    uint8_t ID;
    float position;
    float speed;
};

struct RCP_TransducerData {
    uint32_t timestamp;
    uint8_t ID;
    float pressure;
};

struct RCP_GPSData {
    uint32_t timestamp;
    float latitude;
    float longitude;
    float altitude;
    float groundSpeed;
};

struct RCP_AxisData {
    uint32_t timestamp;
    float x;
    float y;
    float z;
};

struct RCP_floatData {
    uint32_t timestamp;
    float data;
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
    int (*processStepperData)(struct RCP_StepperData data);
    int (*processTransducerData)(struct RCP_TransducerData data);
    int (*processGPSData)(struct RCP_GPSData data);
    int (*processMagnetometerData)(struct RCP_AxisData data);
    int (*processAMPressureData)(struct RCP_floatData data);
    int (*processAMTemperatureData)(struct RCP_floatData data);
    int (*processHumidityData) (struct RCP_floatData data);
    int (*processAccelerationData)(struct RCP_AxisData data);
    int (*processGyroData)(struct RCP_AxisData data);
    int (*processSerialData)(struct RCP_CustomData data);
};

// Provide library with callbacks to needed functions
int RCP_init(struct RCP_LibInitData callbacks);
int RCP_isOpen();
int RCP_shutdown();

// Library will default to channel zero, but it can be changed here.
void RCP_setChannel(RCP_Channel_t ch);

// Function to call periodically to poll for data
int RCP_poll();

// Functions to send controller packets
int RCP_sendEStop();
int RCP_sendHeartbeat();

int RCP_startTest(uint8_t testnum);
int RCP_setDataStreaming(int datastreaming);
int RCP_changeTestProgress(RCP_TestStateControlMode_t mode);
int RCP_setHeartbeatTime(uint8_t heartbeatTime);
int RCP_requestTestState();

int RCP_sendSolenoidWrite(uint8_t ID, RCP_SolenoidState_t state);
int RCP_requestSolenoidRead(uint8_t ID);

int RCP_sendStepperWrite(uint8_t ID, RCP_StepperControlMode_t mode, int32_t value);
int RCP_requestStepperRead(uint8_t ID);

int RCP_requestDeviceReadNOID(RCP_DeviceClass_t device);
int RCP_requestDeviceReadID(RCP_DeviceClass_t device, uint8_t ID);

int RCP_sendRawSerial(const uint8_t* data, uint8_t size);

#ifdef __cplusplus
}
#endif

#endif //RCP_HOST_H
