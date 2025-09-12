#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/time.h>

// Protocol constants
#define PACKET_SIZE 1024
#define WINDOW_SIZE 10
#define RTO_MS 500  // Retransmission timeout in milliseconds
#define BUFFER_SIZE (WINDOW_SIZE * PACKET_SIZE * 2)  // Receiver buffer size in bytes

// Packet types
#define DATA 0x08
#define CHAT 0x10

typedef struct sham_header {
    uint32_t seq_num;     // Sequence Number
    uint32_t ack_num;     // Acknowledgment Number
    uint16_t flags;       // Control flags (SYN, ACK, FIN, DATA)
    uint16_t window_size; // Flow control window size
    uint16_t data_size;   // Actual size of data in this packet
    uint16_t padding;     // For alignment
} __attribute__((packed)) header;

typedef struct sham_packet {
    header hdr;
    char data[PACKET_SIZE];
} __attribute__((packed)) packet;

typedef struct window_slot {
    packet pkt;
    struct timeval sent_time;
    int is_acked;
    int retransmissions;
} window_slot;

// Receiver's buffer for out-of-order packets
typedef struct recv_buffer {
    packet *packets;
    int *received;
    uint32_t base;       // Smallest unacked sequence number
    uint32_t next_seq;   // Next sequence number expected
    int capacity;
    int bytes_buffered;  // Current bytes buffered (for flow control)
    int max_buffer_size; // Maximum buffer size in bytes
} recv_buffer;

// Message queue structure for chat
typedef struct message_queue {
    packet messages[10];  // Buffer for up to 10 pending messages
    int front, rear, count;
    uint32_t next_seq_to_send;
} message_queue;

// Function declarations
recv_buffer* init_recv_buffer(int capacity);
void free_recv_buffer(recv_buffer* rb);
void init_message_queue(message_queue *mq);
int is_queue_full(message_queue *mq);
int is_queue_empty(message_queue *mq);
void enqueue_message(message_queue *mq, const char *message);