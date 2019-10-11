#include "proto.h"

#include "adc.h"
#include "utils.h"

#include <time.h>

/* Get current time macro */
#define GET_TIME() ((uint32_t)(clock() * 1000 / CLOCKS_PER_SEC))

/* Protocol status structure 
 * Maintaining the state between commands
 */
proto_stat stats;

uint32_t last_valid_message_time;

uint8_t proto_verify_crc(uint8_t *message, uint8_t size) {
    unsigned long cksm = 0;
    for (int i = 2; i < size - 2; i++)
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

// {0x55, 0xAA, 0x4, 0x20, 0x3, 0x7C, 0x0, 0x0, 0x5C, 0xFF};

static uint8_t connected() {
    /* Some way to measure the timeout, in case connection dropped */

    if (GET_TIME() - last_valid_message_time >= COMM_TIMEOUT) {
        /* Something happened, reset the connection */
        return 0;
    }

    return 1;
}

static void process_command(const uint8_t *command, uint16_t size) {
    
    // print_command(command, size);
    // 55 aa 06 21 64 00 00 07 00 02 6b ff
    /* Verify crc */
    if (!proto_verify_crc(command, size))
        return;


    /* Update last good message */
    last_valid_message_time = GET_TIME();

    switch (command[3]) {
        case 0x21:
            switch (command[4]) {
                case 0x01:
                    if (command[4] == 0x61)
                        stats.tail = command[5];
                    break;
                case 0x64:
                    stats.eco = command[6];
                    stats.led = command[7];
                    stats.night = command[8];
                    stats.beep = command[9];
                    break;
            }
            break;
        case 0x23:
            switch (command[5]) {
                case 0x7B:
                    stats.ecoMode = command[6];
                    stats.cruise = command[8];
                    break;
                case 0x7D:
                    stats.tail = command[6];
                    break;
                case 0xB0:
                    stats.alarmStatus = command[8];
                    stats.lock = command[10];
                    stats.battery = command[14];
                    stats.velocity = (command[16] + (command[17] * 256)) / 1000 / PROTO_CONSTANT;
                    stats.averageVelocity = (command[18] + (command[19] * 265)) / 1000 / PROTO_CONSTANT;
                    stats.odometer = (command[20] + (command[21] * 256) + (command[22] * 256 * 256)) / 1000 / PROTO_CONSTANT;
                    stats.temperature = ((command[28] + (command[29] * 256)) / 10 * 9 / 5) + 32;
                    break;
            }
    }

    printf("State \n");
    printf("tail: %hhu \n", stats.tail);
    printf("eco: %hhu \n", stats.eco);
    printf("led: %hhu \n", stats.led);
    printf("night: %hhu \n", stats.night);
    printf("beep: %hhu \n", stats.beep);
    printf("eco: %hhu \n", stats.ecoMode);
    printf("cruise: %hhu \n", stats.cruise);
    printf("lock: %hhu \n", stats.lock);
    printf("battery: %hhu \n", stats.battery);
    printf("velocity: %hhu \n", stats.velocity);
    printf("averageVelocity: %hhu \n", stats.averageVelocity);
    printf("odometer: %hhu \n", stats.odometer);
    printf("temperature: %hhu \n", stats.temperature);

    if(stats.alarmStatus) {
        printf("Beeeep \n");
        stats.alarmStatus = 0;
    }

}

static void process_buffer(comm_chan *channel, QueueHandle_t display_queue) {
    uint16_t buffer_size = channel->rx_size - channel->tx_size;
    printf("buffer_size %hu %hu %hu \n", channel->rx_size, channel->tx_size, buffer_size);
    /* Hack to eliminate what it's send on tx */
    if (buffer_size <= 0 || buffer_size > COMM_BUFF_SIZE) {
        /* Nothing to receive */
        return;
    }

    uint8_t *buffer = channel->rx + channel->tx_size;

    /* Multiple commands in same buffer */
    uint8_t *command_ptr = buffer;
    uint16_t command_size = 0;

    uint16_t command_no = 0;
    uint16_t index = 0;

    while (index < buffer_size) {
        if (index &&
            buffer[index - 1] == PROTO_COMMAND_HEADER0 &&
            buffer[index] == PROTO_COMMAND_HEADER1) {
            /* We have a command header */
            if (command_no > 0) {
                command_size = index - 1 - command_size;
                process_command(command_ptr, command_size);
                command_ptr = buffer + index - 1;
            }
            /* Anohter command is found */
            command_no++;
        }
        else if (index == buffer_size - 1) {
            /* Send last packet */
            command_size = index - command_size + 1;
            process_command(command_ptr, command_size);
            
        }
        index++;
    }
}

void proto_command(comm_chan *channel, QueueHandle_t display_queue) {
    static uint8_t messageType = 0;

    uint8_t brake = adc_brake();
    uint8_t speed = adc_speed();

    process_buffer(channel, display_queue);

    if (!connected()) {
        printf("Not connected \n");
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
            // printf("SEND DATA Command %d \n", messageType - 1);
            proto_add_crc(command, sizeof(command));
            comm_copy_tx_chan(channel, command, sizeof(command));
            if(stats.beep) stats.beep = 0;
            break;
        }
        case 4: {
            uint8_t command[] = {0x55, 0xAA, 0x9, 0x20, 0x64, 0x0, 0x6, speed, brake, 0x0, stats.beep, 0x72, 0x0, 0x0, 0x0};
            proto_add_crc(command, sizeof(command));
            // printf("CONNECT Command %d \n", messageType - 1);
            // printf("Command size %d \n", sizeof(command));
            comm_copy_tx_chan(channel, command, sizeof(command));
            if(stats.beep) stats.beep = 0;
            break;
        }
        case 5: {
            uint8_t command[] = {0x55, 0xAA, 0x6, 0x20, 0x61, 0xB0, 0x20, 0x02, speed, brake, 0x0, 0x0};
            proto_add_crc(command, sizeof(command));
            // printf("SHIT1 Command %d \n", messageType - 1);
            comm_copy_tx_chan(channel, command, sizeof(command));
            break;
        }
        case 6: {
            uint8_t command[] = {0x55, 0xAA, 0x6, 0x20, 0x61, 0x7B, 0x4, 0x2, speed, brake, 0x0, 0x0};
            proto_add_crc(command, sizeof(command));
            // printf("SHIT2 Command %d \n", messageType - 1);
            comm_copy_tx_chan(channel, command, sizeof(command));
            break;
        }
        case 7: {
            uint8_t command[] = {0x55, 0xAA, 0x6, 0x20, 0x61, 0x7D, 0x2, 0x2, speed, brake, 0x0, 0x0};
            proto_add_crc(command, sizeof(command));
            // printf("SHIT3 Command %d \n", messageType - 1);
            comm_copy_tx_chan(channel, command, sizeof(command));
            messageType = 0;
            break;
        }
    }
}