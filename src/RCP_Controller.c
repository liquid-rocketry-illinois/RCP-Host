#include "RCP_Controller/RCP_Controller.h"

#include <stdbool.h>

#ifdef __cplusplus
namespace LRI::RCP {
#endif

    Channel channel = CH_ZERO;

    void setChannel(Channel ch) {
        channel = ch;
    }

    int sendEStop() {
        uint8_t ESTOP = channel & 0x3F;
        return sendData(&ESTOP, 1) == 1 ? 0 : -1;
    }

    int sendTestUpdate(TestStateControl state, uint8_t testId) {
        uint8_t buffer[3] = {0};
        buffer[0] = channel & 0x02;
        buffer[1] = CONTROLLER_CLASS_TESTING_WRITE;
        buffer[2] = state & testId;
        return sendData(buffer, 3) == 3 ? 0 : -1;
    }

    int requestTestUpdate() {
        uint8_t buffer[2] = {0};
        buffer[0] = channel & 0x01;
        buffer[1] = CONTROLLER_CLASS_TESTING_READ;
        return sendData(buffer, 2) == 2 ? 0 : -1;
    }

    int sendSolenoidWrite(uint8_t ID, SolenoidState state) {
        uint8_t buffer[3] = {0};
        buffer[0] = channel & 0x02;
        buffer[1] = CONTROLLER_CLASS_SOLENOID_WRITE;
        buffer[2] = state & ID;
        return sendData(buffer, 3) == 3 ? 0 : -1;
    }

    int requestSolenoidRead(uint8_t ID) {
        uint8_t buffer[3] = {0};
        buffer[0] = channel & 0x02;
        buffer[1] = CONTROLLER_CLASS_SOLENOID_READ;
        buffer[2] = ID;
        return sendData(buffer, 3) == 3 ? 0 : -1;
    }

    int sendStepperWrite(uint8_t ID, StepperWriteMode mode, int32_t value) {
        uint8_t buffer[7] = {0};
        buffer[0] = channel & 0x06;
        buffer[1] = CONTROLLER_CLASS_STEPPER_WRITE;
        buffer[2] = mode & ID;
        buffer[3] = value >> 24;
        buffer[4] = (value >> 16) & 0xFF;
        buffer[5] = (value >> 8) & 0xFF;
        buffer[6] = value & 0xFF;
        return sendData(buffer, 7) == 7 ? 0 : -1;
    }

    int requestStepperRead(uint8_t ID) {
        uint8_t buffer[3] = {0};
        buffer[0] = channel & 0x02;
        buffer[1] = CONTROLLER_CLASS_STEPPER_READ;
        buffer[2] = ID;
        return sendData(buffer, 3) == 3 ? 0 : -1;
    }

#ifdef __cplusplus
}
#endif