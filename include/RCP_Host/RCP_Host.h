#ifndef RCP_HOST_H
#define RCP_HOST_H

#include <stddef.h>
#include <stdint.h>

typedef uint8_t RCP_Channel_t;

enum RCP_Channel {
    CH_ZERO = 0x00,
    CH_ONE = 0x40,
    CH_TWO = 0x80,
    CH_THREE = 0xC0,
    CHANNEL_MASK = 0xC0,
};

typedef uint8_t RCP_HostClass_t;

enum RCP_HostClass {
    HOST_CLASS_TESTING_WRITE = 0x00,
    HOST_CLASS_TESTING_READ = 0xF0,
    HOST_CLASS_SOLENOID_WRITE = 0x01,
    HOST_CLASS_SOLENOID_READ = 0xF1,
    HOST_CLASS_STEPPER_WRITE = 0x02,
    HOST_CLASS_STEPPER_READ = 0xF2,
};

typedef uint8_t RCP_TargetClass_t;

enum RCP_TargetClass {
    TARGET_CLASS_TESTING_DATA = 0x00,
    TARGET_CLASS_SOLENOID_DATA = 0x01,
    TARGET_CLASS_STEPPER_DATA = 0x02,
    TARGET_CLASS_TRANSDUCER_DATA = 0x43,
    TARGET_CLASS_GPS_DATA = 0x80,
    TARGET_CLASS_MAGNETOMETER_DATA = 0x81,
    TARGET_CLASS_AM_PRESSURE_DATA = 0x82,
    TARGET_CLASS_AM_TEMPERATURE_DATA = 0x83,
    TARGET_CLASS_ACCELERATION_DATA = 0x84,
    TARGET_CLASS_GYRO_DATA = 0x85,
    TARGET_CLASS_RAW_SERIAL = 0xFF,
};

typedef uint8_t RCP_TestStateControl_t;

enum RCP_TestStateControl {
    TEST_START = 0x00,
    TEST_STOP = 0x10,
    TEST_PAUSE = 0x20,
    DATA_STREAM_START = 0x30,
    DATA_STREAM_STOP = 0x40,
};

typedef uint8_t RCP_TestRunningState_t;

enum RCP_TestRunningState {
    TEST_RUNNING = 0x00,
    TEST_STOPPED = 0x10,
    TEST_PAUSED = 0x20,
    TEST_ESTOP = 0x30,
    TEST_STATE_MASK = 0xF0,
};

typedef uint8_t RCP_SolenoidState_t;

enum RCP_SolenoidState {
    SOLENOID_ON = 0x40,
    SOLENOID_OFF = 0x80,
    SOLENOID_TOGGLE = 0xC0,
    SOLENOID_STATE_MASK = 0xC0,
};

typedef uint8_t RCP_StepperWriteMode_t;

enum RCP_StepperWriteMode {
    ABSOLUTE_POSITION = 0x00,
    RELATIVE_POSITION = 0x40,
    SPEED_CONTROL = 0x80,
};

struct RCP_TestData {
    int32_t timestamp;
    int dataStreaming;
    RCP_TestRunningState_t state;
    uint8_t selectedTest;
};

struct RCP_SolenoidData {
    int32_t timestamp;
    uint8_t ID;
    RCP_SolenoidState_t state;
};

struct RCP_StepperData {
    int32_t timestamp;
    uint8_t ID;
    int32_t position;
    int32_t speed;
};

struct RCP_TransducerData {
    int32_t timestamp;
    uint8_t ID;
    int32_t pressure;
};

struct RCP_GPSData {
    int32_t timestamp;
    int32_t latitude;
    int32_t longitude;
    int32_t altitude;
    int32_t groundSpeed;
};

struct RCP_AxisData {
    int32_t timestamp;
    int32_t x;
    int32_t y;
    int32_t z;
};

struct RCP_AMPressureData {
    int32_t timestamp;
    int32_t pressure;
};

struct RCP_AMTemperatureData {
    int32_t timestamp;
    int32_t temperature;
};

struct RCP_SerialData {
    int32_t timestamp;
    void* data;
    uint8_t size;
};

struct RCP_LibInitData {
    size_t (*sendData)(const void* data, size_t length);
    size_t (*readData)(void* data, size_t length);
    int (*processTestUpdate)(const struct RCP_TestData data);
    int (*processSolenoidData)(const struct RCP_SolenoidData data);
    int (*processStepperData)(const struct RCP_StepperData data);
    int (*processTransducerData)(const struct RCP_TransducerData data);
    int (*processGPSData)(const struct RCP_GPSData data);
    int (*processMagnetometerData)(const struct RCP_AxisData data);
    int (*processAMPressureData)(const struct RCP_AMPressureData data);
    int (*processAMTemperatureData)(const struct RCP_AMTemperatureData data);
    int (*processAccelerationData)(const struct RCP_AxisData data);
    int (*processGyroData)(const struct RCP_AxisData data);
    int (*processSerialData)(const struct RCP_SerialData data);
};

// Provide library with callbacks to needed functions
int RCP_init(const struct RCP_LibInitData callbacks);

int RCP_shutdown();

// Library will default to channel zero, but it can be changed here.
void RCP_setChannel(RCP_Channel_t ch);

// Function to call periodically to poll for data
int RCP_poll();

// Functions to send controller packets
int RCP_sendEStop();

int RCP_sendTestUpdate(RCP_TestStateControl_t state, uint8_t testId);
int RCP_requestTestUpdate();

int RCP_sendSolenoidWrite(uint8_t ID, RCP_SolenoidState_t state);
int RCP_requestSolenoidRead(uint8_t ID);

int RCP_sendStepperWrite(uint8_t ID, RCP_StepperWriteMode_t mode, int32_t value);
int RCP_requestStepperRead(uint8_t ID);

#endif //RCP_HOST_H
