#include "RCP_Host/RCP_Host.h"

#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
namespace LRI::RCP {
#endif

int32_t toInt32(const uint8_t* start) {
    return (start[0] << 24) | (start[1] << 16) | (start[2] << 8) | start[3];
}

Channel_t channel = CH_ZERO;
struct LibInitData* callbacks = NULL;

int init(const struct LibInitData _callbacks) {
    if(callbacks != NULL) return -1;
    callbacks = malloc(sizeof(struct LibInitData));
    *callbacks = _callbacks;
    return 0;
}

int shutdown() {
    if(callbacks == NULL) return -1;
    free(callbacks);
    return 0;
}

void setChannel(Channel_t ch) {
    channel = ch;
}

int poll() {
    if(callbacks == NULL) return -2;
    uint8_t buffer[64] = {0};
    int bread = callbacks->readData(buffer, 1);
    if(bread < 1) return -1;
    uint8_t pktlen = buffer[0] & ~CHANNEL_MASK;
    bread = callbacks->readData(buffer + 1, pktlen);
    if(bread < pktlen) return -1;
    if(buffer[0] & CHANNEL_MASK != channel) return 0;

    uint32_t timestamp = (buffer[2] << 24) | (buffer[3] << 16) | (buffer[4] << 8) | buffer[5];
    switch(buffer[1]) {
    case TARGET_CLASS_TESTING_DATA: {
        struct TestData d = {
            .timestamp = timestamp,
            .dataStreaming = buffer[6] & 0x80,
            .state = buffer[6] & TEST_STATE_MASK,
            .selectedTest = buffer[6] & 0x0F
        };

        callbacks->processTestUpdate(d);
        break;
    }

    case TARGET_CLASS_SOLENOID_DATA: {
        struct SolenoidData d = {
            .timestamp = timestamp,
            .state = buffer[6] & SOLENOID_STATE_MASK,
            .ID = buffer[6] & ~SOLENOID_STATE_MASK
        };

        callbacks->processSolenoidData(d);
        break;
    }

    case TARGET_CLASS_STEPPER_DATA: {
        struct StepperData d = {
            .timestamp = timestamp,
            .ID = buffer[6],
            .position = toInt32(buffer + 7),
            .speed = toInt32(buffer + 11)
        };

        callbacks->processStepperData(d);
        break;
    }

    case TARGET_CLASS_TRANSDUCER_DATA: {
        struct TransducerData d = {
            .timestamp = timestamp,
            .ID = buffer[6],
            .pressure = toInt32(buffer + 7)
        };

        callbacks->processTransducerData(d);
        break;
    }

    case TARGET_CLASS_GPS_DATA: {
        struct GPSData d = {
            .timestamp = timestamp,
            .latitude = toInt32(buffer + 6),
            .longitude = toInt32(buffer + 10),
            .altitude = toInt32(buffer + 14),
            .groundSpeed = toInt32(buffer + 18)
        };

        callbacks->processGPSData(d);
        break;
    }

    case TARGET_CLASS_AM_PRESSURE_DATA: {
        struct AMPressureData d = {
            .timestamp = timestamp,
            .pressure = toInt32(buffer + 6)
        };

        callbacks->processAMPressureData(d);
        break;
    }

    case TARGET_CLASS_AM_TEMPERATURE_DATA: {
        struct AMTemperatureData d = {
            .timestamp = timestamp,
            .temperature = toInt32(buffer + 6)
        };

        callbacks->processAMTemperatureData(d);
        break;
    }

    case TARGET_CLASS_ACCELERATION_DATA:
    case TARGET_CLASS_MAGNETOMETER_DATA:
    case TARGET_CLASS_GYRO_DATA: {
        struct AxisData d = {
            .timestamp = timestamp,
            .x = toInt32(buffer + 6),
            .y = toInt32(buffer + 10),
            .z = toInt32(buffer + 14)
        };

        (buffer[1] == TARGET_CLASS_GYRO_DATA
             ? callbacks->processGyroData
             : buffer[1] == TARGET_CLASS_ACCELERATION_DATA
             ? callbacks->processAccelerationData
             : callbacks->processMagnetometerData)(d);
        break;
    }

    case TARGET_CLASS_RAW_SERIAL: {
        uint8_t* sdata = (uint8_t*)malloc(pktlen - 5);
        memcpy(sdata, buffer + 6, pktlen - 5);
        struct SerialData d = {
            .timestamp = timestamp,
            .size = pktlen - 5,
            .data = sdata
        };

        callbacks->processSerialData(d);
        break;
    }

    default:
        break;
    }

    return 0;
}

int sendEStop() {
    if(callbacks == NULL) return -2;
    uint8_t ESTOP = channel | 0x00;
    return callbacks->sendData(&ESTOP, 1) == 1 ? 0 : -1;
}

int sendTestUpdate(TestStateControl_t state, uint8_t testId) {
    if(callbacks == NULL) return -2;
    uint8_t buffer[3] = {0};
    buffer[0] = channel & 0x02;
    buffer[1] = HOST_CLASS_TESTING_WRITE;
    buffer[2] = state & testId;
    return callbacks->sendData(buffer, 3) == 3 ? 0 : -1;
}

int requestTestUpdate() {
    if(callbacks == NULL) return -2;
    uint8_t buffer[2] = {0};
    buffer[0] = channel & 0x01;
    buffer[1] = HOST_CLASS_TESTING_READ;
    return callbacks->sendData(buffer, 2) == 2 ? 0 : -1;
}

int sendSolenoidWrite(uint8_t ID, SolenoidState_t state) {
    if(callbacks == NULL) return -2;
    uint8_t buffer[3] = {0};
    buffer[0] = channel & 0x02;
    buffer[1] = HOST_CLASS_SOLENOID_WRITE;
    buffer[2] = state & ID;
    return callbacks->sendData(buffer, 3) == 3 ? 0 : -1;
}

int requestSolenoidRead(uint8_t ID) {
    if(callbacks == NULL) return -2;
    uint8_t buffer[3] = {0};
    buffer[0] = channel & 0x02;
    buffer[1] = HOST_CLASS_SOLENOID_READ;
    buffer[2] = ID;
    return callbacks->sendData(buffer, 3) == 3 ? 0 : -1;
}

int sendStepperWrite(uint8_t ID, StepperWriteMode_t mode, int32_t value) {
    if(callbacks == NULL) return -2;
    uint8_t buffer[7] = {0};
    buffer[0] = channel & 0x06;
    buffer[1] = HOST_CLASS_STEPPER_WRITE;
    buffer[2] = mode & ID;
    buffer[3] = value >> 24;
    buffer[4] = value >> 16;
    buffer[5] = value >> 8;
    buffer[6] = value;
    return callbacks->sendData(buffer, 7) == 7 ? 0 : -1;
}

int requestStepperRead(uint8_t ID) {
    if(callbacks == NULL) return -2;
    uint8_t buffer[3] = {0};
    buffer[0] = channel & 0x02;
    buffer[1] = HOST_CLASS_STEPPER_READ;
    buffer[2] = ID;
    return callbacks->sendData(buffer, 3) == 3 ? 0 : -1;
}

#ifdef __cplusplus
}
#endif
