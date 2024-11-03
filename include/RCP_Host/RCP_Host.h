#ifndef RCP_HOST_H
#define RCP_HOST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
namespace LRI::RCP {
#endif

    typedef uint8_t Channel;
    enum Channel {
        CH_ZERO      = 0x00,
        CH_ONE       = 0x40,
        CH_TWO       = 0x80,
        CH_THREE     = 0xC0,
        CHANNEL_MASK = 0xC0,
    };

    typedef uint8_t HostClass;
    enum HostClass {
        HOST_CLASS_TESTING_WRITE  = 0x00,
        HOST_CLASS_TESTING_READ   = 0xF0,
        HOST_CLASS_SOLENOID_WRITE = 0x01,
        HOST_CLASS_SOLENOID_READ  = 0xF1,
        HOST_CLASS_STEPPER_WRITE  = 0x02,
        HOST_CLASS_STEPPER_READ   = 0xF2,
    };

    typedef uint8_t TargetClass;
    enum TargetClass {
        TARGET_CLASS_TESTING_DATA        = 0x00,
        TARGET_CLASS_SOLENOID_DATA       = 0x01,
        TARGET_CLASS_STEPPER_DATA        = 0x02,
        TARGET_CLASS_TRANSDUCER_DATA     = 0x43,
        TARGET_CLASS_GPS_DATA            = 0x80,
        TARGET_CLASS_MAGNETOMETER_DATA   = 0x81,
        TARGET_CLASS_AM_PRESSURE_DATA    = 0x82,
        TARGET_CLASS_AM_TEMPERATURE_DATA = 0x83,
        TARGET_CLASS_ACCELERATION_DATA   = 0x84,
        TARGET_CLASS_GYRO_DATA           = 0x85,
        TARGET_CLASS_RAW_SERIAL          = 0xFF,

    };

    typedef uint8_t TestStateControl;
    enum TestStateControl {
        TEST_START        = 0x00,
        TEST_STOP         = 0x10,
        TEST_PAUSE        = 0x20,
        DATA_STREAM_START = 0x30,
        DATA_STREAM_STOP  = 0x40,
    };

    typedef uint8_t TestRunningState;
    enum TestRunningState {
        TEST_RUNNING    = 0x00,
        TEST_STOPPED    = 0x10,
        TEST_PAUSED     = 0x20,
        TEST_ESTOP      = 0x30,
        TEST_STATE_MASK = 0xF0,
    };

    typedef uint8_t SolenoidState;
    enum SolenoidState {
        SOLENOID_ON         = 0x40,
        SOLENOID_OFF        = 0x80,
        SOLENOID_TOGGLE     = 0xC0,
        SOLENOID_STATE_MASK = 0xC0,
    };

    typedef uint8_t StepperWriteMode;
    enum StepperWriteMode {
        ABSOLUTE_POSITION = 0x00,
        RELATIVE_POSITION = 0x40,
        SPEED_CONTROL     = 0x80,
    };

    struct TestData {
        int32_t timestamp;
        int dataStreaming;
        TestRunningState state;
        uint8_t selectedTest;
    };

    struct SolenoidData {
        int32_t timestamp;
        uint8_t ID;
        SolenoidState state;
    };

    struct StepperData {
        int32_t timestamp;
        uint8_t ID;
        int32_t position;
        int32_t speed;
    };

    struct TransducerData {
        int32_t timestamp;
        uint8_t ID;
        int32_t pressure;
    };

    struct GPSData {
        int32_t timestamp;
        int32_t latitude;
        int32_t longitude;
        int32_t altitude;
        int32_t groundSpeed;
    };

    struct AxisData {
        int32_t timestamp;
        int32_t x;
        int32_t y;
        int32_t z;
    };

    struct AMPressureData {
        int32_t timestamp;
        int32_t pressure;
    };

    struct AMTemperatureData {
        int32_t timestamp;
        int32_t temperature;
    };

    struct SerialData {
        int32_t timestamp;
        void* data;
        uint8_t size;
    };

    struct LibInitData {
        size_t (*sendData) (const void* data, size_t length);
        size_t (*readData) (const void* data, size_t length);
        int (*processTestUpdate) (const struct TestData data);
        int (*processSolenoidData) (const struct SolenoidData data);
        int (*processStepperData) (const struct StepperData data);
        int (*processTransducerData) (const struct TransducerData data);
        int (*processGPSData) (const struct GPSData data);
        int (*processMagnetometerData) (const struct AxisData data);
        int (*processAMPressureData) (const struct AMPressureData data);
        int (*processAMTemperatureData) (const struct AMTemperatureData data);
        int (*processAccelerationData) (const struct AxisData data);
        int (*processGyroData) (const struct AxisData data);
        int (*processSerialData) (const struct SerialData data);
    };

    // Provide library with callbacks to needed functions
    int init(const struct LibInitData callbacks);

    int shutdown();

    // Library will default to channel zero, but it can be changed here.
    void setChannel(Channel ch);

    // Function to call periodically to poll for data
    int poll();

    // Functions to send controller packets
    int sendEStop();

    int sendTestUpdate(TestStateControl state, uint8_t testId = 0);
    int requestTestUpdate();

    int sendSolenoidWrite(uint8_t ID, SolenoidState state);
    int requestSolenoidRead(uint8_t ID);

    int sendStepperWrite(uint8_t ID, StepperWriteMode mode, int32_t value);
    int requestStepperRead(uint8_t ID);

#ifdef __cplusplus
}
#endif

#endif //RCP_HOST_H
