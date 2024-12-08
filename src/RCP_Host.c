#include "RCP_Host/RCP_Host.h"

#include <stdlib.h>
#include <string.h>

int32_t toInt32(const uint8_t* start) {
    return (start[0] << 24) | (start[1] << 16) | (start[2] << 8) | start[3];
}

RCP_Channel_t channel = RCP_CH_ZERO;
struct RCP_LibInitData* callbacks = NULL;

int RCP_init(const struct RCP_LibInitData _callbacks) {
    if(callbacks != NULL) return -1;
    callbacks = malloc(sizeof(struct RCP_LibInitData));
    *callbacks = _callbacks;
    return 0;
}

int RCP_isOpen() {
    return callbacks != NULL;
}

int RCP_shutdown() {
    if(callbacks == NULL) return -1;
    free(callbacks);
    callbacks = NULL;
    return 0;
}

void RCP_setChannel(RCP_Channel_t ch) {
    channel = ch;
}

int RCP_poll() {
    if(callbacks == NULL) return -2;
    uint8_t buffer[64] = {0};
    int bread = callbacks->readData(buffer, 1);
    if(bread != 1) return -1;
    uint8_t pktlen = buffer[0] & ~RCP_CHANNEL_MASK;
    bread = callbacks->readData(buffer + 1, pktlen + 1);
    if(bread != pktlen + 1) return -1;
    if((buffer[0] & RCP_CHANNEL_MASK) != channel) return 0;

    uint32_t timestamp = toInt32(buffer + 2);

    switch(buffer[1]) {
    case RCP_DEVCLASS_TEST_STATE: {
        struct RCP_TestData d = {
            .timestamp = timestamp,
            .dataStreaming = buffer[6] & 0x80,
            .state = buffer[6] & RCP_TEST_STATE_MASK,
            .heartbeatTime = buffer[6] & 0x0F
        };

        callbacks->processTestUpdate(d);
        break;
    }

    case RCP_DEVCLASS_SOLENOID: {
        struct RCP_SolenoidData d = {
            .timestamp = timestamp,
            .state = buffer[6] & RCP_SOLENOID_STATE_MASK,
            .ID = buffer[6] & ~RCP_SOLENOID_STATE_MASK
        };

        callbacks->processSolenoidData(d);
        break;
    }

    case RCP_DEVCLASS_STEPPER: {
        struct RCP_StepperData d = {
            .timestamp = timestamp,
            .ID = buffer[6],
            .position = toInt32(buffer + 7),
            .speed = toInt32(buffer + 11)
        };

        callbacks->processStepperData(d);
        break;
    }

    case RCP_DEVCLASS_PRESSURE_TRANSDUCER: {
        struct RCP_TransducerData d = {
            .timestamp = timestamp,
            .ID = buffer[6],
            .pressure = toInt32(buffer + 7)
        };

        callbacks->processTransducerData(d);
        break;
    }

    case RCP_DEVCLASS_GPS: {
        struct RCP_GPSData d = {
            .timestamp = timestamp,
            .latitude = toInt32(buffer + 6),
            .longitude = toInt32(buffer + 10),
            .altitude = toInt32(buffer + 14),
            .groundSpeed = toInt32(buffer + 18)
        };

        callbacks->processGPSData(d);
        break;
    }

    case RCP_DEVCLASS_AM_PRESSURE: {
        struct RCP_AMPressureData d = {
            .timestamp = timestamp,
            .pressure = toInt32(buffer + 6)
        };

        callbacks->processAMPressureData(d);
        break;
    }

    case RCP_DEVCLASS_AM_TEMPERATURE: {
        struct RCP_AMTemperatureData d = {
            .timestamp = timestamp,
            .temperature = toInt32(buffer + 6)
        };

        callbacks->processAMTemperatureData(d);
        break;
    }

    case RCP_DEVCLASS_ACCELEROMETER:
    case RCP_DEVCLASS_MAGNETOMETER:
    case RCP_DEVCLASS_GYROSCOPE: {
        struct RCP_AxisData d = {
            .timestamp = timestamp,
            .x = toInt32(buffer + 6),
            .y = toInt32(buffer + 10),
            .z = toInt32(buffer + 14)
        };

        (buffer[1] == RCP_DEVCLASS_GYROSCOPE
             ? callbacks->processGyroData
             : buffer[1] == RCP_DEVCLASS_ACCELEROMETER
             ? callbacks->processAccelerationData
             : callbacks->processMagnetometerData)(d);
        break;
    }

    case RCP_DEVCLASS_CUSTOM: {
        uint8_t* sdata = (uint8_t*)malloc(pktlen - 4);
        memcpy(sdata, buffer + 6, pktlen - 4);
        struct RCP_CustomData d = {
            .timestamp = timestamp,
            .length = pktlen - 4,
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

int RCP_sendEStop() {
    if(callbacks == NULL) return -2;
    uint8_t ESTOP = channel | 0x00;
    return callbacks->sendData(&ESTOP, 1) == 1 ? 0 : -1;
}

int __RCP_sendTestUpdate(RCP_TestStateControlMode_t mode, uint8_t param) {
    if(callbacks == NULL) return -2;
    uint8_t buffer[3] = {0};
    buffer[0] = channel | 0x01;
    buffer[1] = RCP_DEVCLASS_TEST_STATE;
    buffer[2] = mode | param;
    return callbacks->sendData(buffer, 3) == 3 ? 0 : -1;
}

int RCP_sendHeartbeat() {
    return __RCP_sendTestUpdate(RCP_HEARTBEATS_CONTROL, 0x0F);
}

int RCP_startTest(uint8_t testnum) {
    return __RCP_sendTestUpdate(RCP_TEST_START, testnum);
}

int RCP_setDataStreaming(int datastreaming) {
    return __RCP_sendTestUpdate(datastreaming ? RCP_DATA_STREAM_START : RCP_DATA_STREAM_STOP, 0);
}

int RCP_changeTestProgress(RCP_TestStateControlMode_t mode) {
    if(mode != RCP_TEST_STOP && mode != RCP_TEST_PAUSE) return -3;
    return __RCP_sendTestUpdate(mode, 0);
}

int RCP_setHeartbeatTime(uint8_t heartbeatTime) {
    return __RCP_sendTestUpdate(RCP_HEARTBEATS_CONTROL, (heartbeatTime & 0x0F));
}

int RCP_requestTestState() {
    return __RCP_sendTestUpdate(RCP_TEST_QUERY, 0);
}

int RCP_sendSolenoidWrite(uint8_t ID, RCP_SolenoidState_t state) {
    if(callbacks == NULL) return -2;
    uint8_t buffer[3] = {0};
    buffer[0] = channel | 0x01;
    buffer[1] = RCP_DEVCLASS_SOLENOID;
    buffer[2] = (state & RCP_SOLENOID_STATE_MASK) | (ID & 0x3F);
    return callbacks->sendData(buffer, 3) == 3 ? 0 : -1;
}

int RCP_requestSolenoidRead(uint8_t ID) {
    if(callbacks == NULL) return -2;
    uint8_t buffer[3] = {0};
    buffer[0] = channel | 0x01;
    buffer[1] = RCP_DEVCLASS_SOLENOID;
    buffer[2] = RCP_SOLENOID_READ | (ID & 0x3F);
    return callbacks->sendData(buffer, 3) == 3 ? 0 : -1;
}

int RCP_sendStepperWrite(uint8_t ID, RCP_StepperControlMode_t mode, int32_t value) {
    if(callbacks == NULL) return -2;
    uint8_t buffer[7] = {0};
    buffer[0] = channel | 0x05;
    buffer[1] = RCP_DEVCLASS_STEPPER;
    buffer[2] = (mode & RCP_STEPPER_CONTROL_MODE_MASK) | (ID & 0x3F);
    buffer[3] = value >> 24;
    buffer[4] = value >> 16;
    buffer[5] = value >> 8;
    buffer[6] = value;
    return callbacks->sendData(buffer, 7) == 7 ? 0 : -1;
}

int RCP_requestStepperRead(uint8_t ID) {
    if(callbacks == NULL) return -2;
    uint8_t buffer[3] = {0};
    buffer[0] = channel | 0x01;
    buffer[1] = RCP_DEVCLASS_STEPPER;
    buffer[2] = RCP_STEPPER_QUERY_STATE | (ID & 0x3F);
    return callbacks->sendData(buffer, 3) == 3 ? 0 : -1;
}

int RCP_requestDeviceReadNOID(RCP_DeviceClass_t device) {
    return RCP_requestDeviceReadID(device, 0);
}

int RCP_requestDeviceReadID(RCP_DeviceClass_t device, uint8_t ID) {
    if(device <= 0x80 || (ID != 0 && device != RCP_DEVCLASS_PRESSURE_TRANSDUCER)) return -3;
    if(callbacks == NULL) return -2;
    uint8_t buffer[3] = {0};
    buffer[0] = channel | 0x01;
    buffer[1] = device;
    buffer[2] = ID;
    return callbacks->sendData(buffer, 3) == 3 ? 0 : -1;
}