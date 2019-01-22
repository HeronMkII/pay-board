/*
Defines the logical interface that PAY uses for CAN communication. This is for
handling received CAN messages, performing actions, and responding.

Authors: Bruno Almeida
*/

#include "can_commands.h"


/* Message queues */

// CAN messages received but not processed yet
queue_t rx_msg_queue;
// CAN messages to transmit
queue_t tx_msg_queue;

// Set this to true to simulate performing all local actions (e.g. fetching
// data, actuating motors) - this allows testing just the PAY command handling
// system on any PCB without any peripherals
bool sim_local_actions = false;

void handle_hk(uint8_t* rx_msg);
void handle_opt(uint8_t* rx_msg);
void handle_exp(uint8_t* rx_msg);


void handle_rx_msg(void) {
    // Get received message from queue
    uint8_t rx_msg[8] = { 0x00 };
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if (queue_empty(&rx_msg_queue)) {
            return;
        }
        dequeue(&rx_msg_queue, rx_msg);
    }

    // Check message type
    switch (rx_msg[1]) {
        case CAN_PAY_HK:
            handle_hk(rx_msg);
            break;
        case CAN_PAY_OPT:
            handle_opt(rx_msg);
            break;
        case CAN_PAY_EXP:
            handle_exp(rx_msg);
            break;
        default:
            return;
    }
}


// Assuming a housekeeping request was received,
// retrieves and places the appropriate data in the tx_data buffer
void handle_hk(uint8_t* rx_msg) {
    uint8_t tx_msg[8] = { 0x00 };
    tx_msg[0] = 0; // TODO
    tx_msg[1] = rx_msg[1];
    tx_msg[2] = rx_msg[2];

    // Declare these here because we can't start a case statement with a declaration
    uint16_t raw_temp_data;
    uint16_t raw_hum_data;
    uint32_t raw_pres_data;

    // Check field number
    switch (rx_msg[2]) {
        case CAN_PAY_HK_TEMP:
            if (sim_local_actions) {
                // 16 bit raw data, make sure the 0b11 on the right is always there
                raw_temp_data = (random() & 0xFFFF) | 0b11;
            } else {
                raw_temp_data = temp_read_raw_data();
            }

            tx_msg[3] = 0x00;
            tx_msg[4] = (raw_temp_data >> 8) & 0xFF;
            tx_msg[5] = raw_temp_data & 0xFF;
            break;

        case CAN_PAY_HK_HUM:
            if (sim_local_actions) {
                // 14 bit raw data
                raw_hum_data = random() & 0x3FFF;
            } else {
                raw_hum_data = hum_read_raw_data();
            }

            tx_msg[3] = 0x00;
            tx_msg[4] = (raw_hum_data >> 8) & 0xFF;
            tx_msg[5] = raw_hum_data & 0xFF;
            break;

        case CAN_PAY_HK_PRES:
            if (sim_local_actions) {
                raw_pres_data = random() & 0xFFFFFF;
            } else {
                raw_pres_data = pres_read_raw_data();
            }

            tx_msg[3] = (raw_pres_data >> 16) & 0xFF;
            tx_msg[4] = (raw_pres_data >> 8) & 0xFF;
            tx_msg[5] = raw_pres_data & 0xFF;
            break;

        default:
            return; // don't send a message back
    }

    // Add message to transmit
    enqueue(&tx_msg_queue, tx_msg);
}


void handle_opt(uint8_t* rx_msg) {
    // Check the field number is valid
    if (rx_msg[2] >= CAN_PAY_SCI_GET_COUNT) {
        return;
    }

    uint32_t raw_optical = 0;

    if (sim_local_actions) {
        // 24 bit raw data
        raw_optical = random() & 0xFFFFFF;
    } else {
        // TODO - higher level optical spi function that delays until interrupts
        // send_read_sensor_command(rx_data[2]);

        // Use random data for now
        raw_optical = random() & 0xFFFFFF;
    }

    // Add a message to transmit back
    uint8_t tx_msg[8] = { 0x00 };
    tx_msg[0] = 0; // TODO
    tx_msg[1] = rx_msg[1];
    tx_msg[2] = rx_msg[2];
    tx_msg[3] = (raw_optical >> 16) & 0xFF;
    tx_msg[4] = (raw_optical >> 8) & 0xFF;
    tx_msg[5] = raw_optical & 0xFF;
    enqueue(&tx_msg_queue, tx_msg);
}


void handle_exp(uint8_t* rx_msg) {
    // Send back the same message type and field number
    uint8_t tx_msg[8] = { 0x00 };
    tx_msg[0] = 0; // TODO
    tx_msg[1] = rx_msg[1];
    tx_msg[2] = rx_msg[2];

    switch(rx_msg[2]) {
        case CAN_PAY_EXP_POP:
            if (!sim_local_actions) {
                // TODO
                // actuate_motors();
            }
            break;
        default:
            return;
    }

    // Enqueue TX message to transmit
    enqueue(&tx_msg_queue, tx_msg);
}
