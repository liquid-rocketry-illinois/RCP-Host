#ifndef RCP_CONTROLLER_H
#define RCP_CONTROLLER_H

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

    typedef uint8_t ControllerClass;
    enum ControllerClass {
        CONTROLLER_CLASS_TESTING_WRITE  = 0x00,
        CONTROLLER_CLASS_TESTING_READ   = 0xF0,
        CONTROLLER_CLASS_SOLENOID_WRITE = 0x01,
        CONTROLLER_CLASS_SOLENOID_READ  = 0xF1,
        CONTROLLER_CLASS_STEPPER_WRITE  = 0x02,
        CONTROLLER_CLASS_STEPPER_READ   = 0xF2,
    };

    typedef uint8_t HostClass;
    enum HostClass {
        HOST_CLASS_TESTING_DATA        = 0x00,
        HOST_CLASS_SOLENOID_DATA       = 0x01,
        HOST_CLASS_STEPPER_DATA        = 0x02,
        HOST_CLASS_TRANSDUCER_DATA     = 0x43,
        HOST_CLASS_GPD_DATA            = 0x80,
        HOST_CLASS_MAGNETOMETER_DATA   = 0x81,
        HOST_CLASS_AM_PRESSURE_DATA    = 0x82,
        HOST_CLASS_AM_TEMPERATURE_DATA = 0x83,
        HOST_CLASS_ACCELERATION_DATA   = 0x84,
        HOST_CLASS_GYRO_DATA           = 0x85,
        HOST_CLASS_RAW_SERIAL          = 0xFF,

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
        int dataStreaming;
        TestRunningState state;
        uint8_t selectedTest;
    };

    struct SolenoidData {
        uint8_t ID;
        SolenoidState state;
    };

    struct StepperData {
        uint8_t ID;
        int32_t position;
        int32_t speed;
    };

    struct TransducerData {
        uint8_t ID;
        int32_t pressure;
    };

    struct GPSData {
        int32_t latitude;
        int32_t longitude;
        int32_t altitude;
        int32_t groundSpeed;
    };

    struct AxisData {
        int32_t x;
        int32_t y;
        int32_t z;
    };

    struct AMPressureData {
        int32_t pressure;
    };

    struct AMTemperatureData {
        int32_t temperature;
    };

    struct SerialData {
        void* data;
        uint8_t size;
    };

    // Stubs that the library user must implement
    size_t sendData(const void* data, size_t length);
    size_t readData(const void* buffer, size_t bufferSize);
    int dataReady();

    int processTestUpdate(struct TestData data);
    int processSolenoidData(struct SolenoidData data);
    int processStepperData(struct StepperData data);
    int processTransducerData(struct TransducerData data);
    int processGPSData(struct GPSData data);
    int processMagnetometerData(struct AxisData data);
    int processAMPressureData(struct AMPressureData data);
    int processAMTemperatureData(struct AMTemperatureData data);
    int processAccelerationData(struct AxisData data);
    int processGyroData(struct AxisData data);
    int processSerialData(struct SerialData data);

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

#endif //RCP_CONTROLLER_H
