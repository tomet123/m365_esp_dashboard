#include "proto.h"

#include "adc.h"

#include <time.h>

/* Get current time macro */
#define GET_TIME() ((uint32_t)(clock() * 1000 / CLOCKS_PER_SEC))

typedef struct __stat {
    uint8_t beep;
} proto_stat;

/* Protocol status structure 
 * Maintaining the state between commands
 */
proto_stat stats;


uint32_t last_valid_message_time;


uint8_t proto_verify_crc(uint8_t *message, uint8_t size) {
    unsigned long cksm = 0;
    for (int i = 0; i < size - 2; i++)
        cksm += message[i];
    cksm ^= 0xFFFF;

    if (cksm == message[size - 2] + (message[size - 1] << 8)) {
        return 1;
    }
    return 0;
}
void proto_add_crc(uint8_t *message, uint8_t size) {
    unsigned long cksm = 0;
    for (int i = 2; i < size - 2; i++)
        cksm += message[i];
    cksm ^= 0xFFFF;
    message[size - 2] = (uint8_t)(cksm & 0xFF);
    message[size - 1] = (uint8_t)((cksm & 0xFF00) >> 8);
}

uint16_t proto_crc(const uint8_t *data, uint16_t size) {
    uint32_t calc = 0;
    uint16_t ret = 0;

    for (uint8_t index = 2; index < size - 2; index++)
        calc += data[index];

    calc ^= 0xffff;
    ret |= (((uint8_t)(calc & 0xff)) << CRC0_SHIFT) & CRC0_MASK;
    ret |= (((uint8_t)((calc & 0xff00) >> 8)) << CRC1_SHIFT) & CRC1_MASK;

    return ret;
}


{0x55, 0xAA, 0x4, 0x20, 0x3, 0x7C, 0x0, 0x0, 0x5C, 0xFF};

static uint8_t connected() {

    /* Some way to measure the timeout, in case connection dropped */

    if (GET_TIME() - last_valid_message_time >= COMM_TIMEOUT) {
        /* Something happened, reset the connection */
        return 0;
    }

    return 1;
}

static void process_command(comm_chan *channel, QueueHandle_t display_queue) {
    /* Verify crc */

    /* Insert display queue if necessary */

    /* If crc status ok */
    last_valid_message_time = GET_TIME(); 

}

void proto_command(comm_chan *channel, QueueHandle_t display_queue) {
    static uint8_t messageType = 0;

    uint8_t brake = adc_brake();
    uint8_t speed = adc_speed();

    /* Read the data, verify crc and send data to the display queue */
    if (channel->rx_size > channel->tx_size) {
        printf("Received something\n");
        uint8_t status;

        /* Process incoming data*/
        process_command(channel, display_queue);
    }

    if (!connected()) {
        messageType = 4;
    }

    /* Iterate over all message types and collect informations */

    switch (messageType++) {
        case 0:
            /* Just for sake of completeness */
        case 1:
            /* Just for sake of completeness */
        case 2:
            /* Just for sake of completeness */
        case 3: {
            /* I don't know what this command does, seems to be the way to actually write the speed and brake values */
            uint8_t command[] = {0x55, 0xAA, 0x7, 0x20, 0x65, 0x0, 0x4, speed, brake, 0x0, stats.beep, 0x0, 0x0};
            proto_add_crc(command, sizeof(command));
            comm_copy_tx_chan(channel, sizeof(command));
            break;
        }
        case 4: {
            uint8_t command[] = {0x55, 0xAA, 0x9, 0x20, 0x64, 0x0, 0x6, speed, brake, 0x0, stats.beep, 0x72, 0x0, 0x0, 0x0};
            proto_add_crc(command, sizeof(command));
            comm_copy_tx_chan(channel, sizeof(command));
            break;
        }
        case 5: {
            uint8_t command[] = {0x55, 0xAA, 0x6, 0x20, 0x61, 0xB0, 0x20, 0x02, speed, brake, 0x0, 0x0};
            proto_add_crc(command, sizeof(command));
            comm_copy_tx_chan(channel, sizeof(command));
            break;
        }
        case 6: {
            uint8_t command[] = {0x55, 0xAA, 0x6, 0x20, 0x61, 0x7B, 0x4, 0x2, speed, brake, 0x0, 0x0};
            proto_add_crc(command, sizeof(command));
            comm_copy_tx_chan(channel, sizeof(command));
            break;
        }
        case 7: {
            uint8_t command[] = {0x55, 0xAA, 0x6, 0x20, 0x61, 0x7D, 0x2, 0x2, speed, brake, 0x0, 0x0};
            proto_add_crc(command, sizeof(command));
            comm_copy_tx_chan(channel, sizeof(command));
            messageType = 0;
            break;
        }
    }
}