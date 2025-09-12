#define _GNU_SOURCE
#include "sham.h"
#include <sys/time.h>
#include <fcntl.h>
#include <stdarg.h>

//llm code starts//

// Verbose logging (file transfer mode only) - declare early
static int LOG_ENABLED = 0;
static FILE *LOG_FP = NULL;
static void log_init_client();
static void log_write(const char *fmt, ...);

// Implement logging helpers early so they are visible to all functions
static void log_init_client() {
    // Enable logging only if RUDP_LOG env var is set (1/true/yes)
    const char *env = getenv("RUDP_LOG");
    int enable = 0;
    if (env) {
        if (strcmp(env, "1") == 0 || strcmp(env, "yes") == 0 || strcmp(env, "YES") == 0 ||
            strcmp(env, "True") == 0 || strcmp(env, "TRUE") == 0 || strcmp(env, "true") == 0) {
                enable = 1;
        }
    }
    if (enable) {
        LOG_FP = fopen("client_log.txt", "w");
        if (LOG_FP) LOG_ENABLED = 1;
    }
}

static void log_write(const char *fmt, ...) {
    if (!LOG_ENABLED || !LOG_FP) return;
    struct timeval tv; gettimeofday(&tv, NULL);
    struct tm tmv; localtime_r(&tv.tv_sec, &tmv);
    char ts[64];
    snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d.%06ld",
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec, (long)tv.tv_usec);
    fprintf(LOG_FP, "[%s] [LOG] ", ts);
    va_list ap; va_start(ap, fmt);
    vfprintf(LOG_FP, fmt, ap);
    va_end(ap);
    fputc('\n', LOG_FP);
    fflush(LOG_FP);
}

//llm code ends//

// Ensure chat_mode is known here
extern int chat_mode;

// Print only in chat mode (for a nicer UI); file mode stays quiet
static void chat_print(const char *fmt, ...) {
    if (!chat_mode) return;
    va_list ap; va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);
}

#define SYN 0x01
#define ACK 0x02
#define FIN 0x04

// Global variables for client configuration
int chat_mode = 0;
float loss_rate = 0.0;
char *server_ip = NULL;
int server_port = 0;
char *input_file = NULL;
char *output_file = NULL;

// Simulate packet loss
int should_drop_packet() {
    if (loss_rate == 0.0) return 0;
    return (rand() / (float)RAND_MAX) < loss_rate;
}

void print_usage(char *program_name) {
    printf("Usage:\n");
    printf("File Transfer Mode: %s <server_ip> <server_port> <input_file> <output_file_name> [loss_rate]\n", program_name);
    printf("Chat Mode: %s <server_ip> <server_port> --chat [loss_rate]\n", program_name);
    printf("\n");
    printf("Arguments:\n");
    printf("  server_ip: IP address of the server\n");
    printf("  server_port: Port number of the server\n");
    printf("  input_file: File to transfer (file mode only)\n");
    printf("  output_file_name: Name for received file (file mode only)\n");
    printf("  --chat: Enable chat mode\n");
    printf("  loss_rate: Optional packet loss rate (0.0-1.0), default 0.0\n");
    exit(EXIT_FAILURE);
}

//llm code starts//

int parse_arguments(int argc, char *argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
    }
    
    // Parse server IP and port
    server_ip = argv[1];
    server_port = atoi(argv[2]);
    if (server_port <= 0 || server_port > 65535) {
        printf("Error: Invalid port number\n");
        exit(EXIT_FAILURE);
    }
    
    // Check if chat mode
    if (argc >= 4 && strcmp(argv[3], "--chat") == 0) {
        chat_mode = 1;
        // Parse optional loss rate for chat mode
        if (argc >= 5) {
            loss_rate = atof(argv[4]);
            if (loss_rate < 0.0 || loss_rate > 1.0) {
                printf("Error: Loss rate must be between 0.0 and 1.0\n");
                exit(EXIT_FAILURE);
            }
        }
    } else {
        // File transfer mode
        if (argc < 5) {
            print_usage(argv[0]);
        }
        input_file = argv[3];
        output_file = argv[4];
        
        // Parse optional loss rate for file mode
        if (argc >= 6) {
            loss_rate = atof(argv[5]);
            if (loss_rate < 0.0 || loss_rate > 1.0) {
                printf("Error: Loss rate must be between 0.0 and 1.0\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    
    return 0;
}

//llm code ends//

//llm code starts//
// Global variables for sliding window
window_slot send_window[WINDOW_SIZE];
uint32_t base = 0;          // Oldest unacked packet
uint32_t next_seq = 0;      // Next sequence number to use
int window_used = 0;        // Number of packets currently in flight
int bytes_in_flight = 0;    // Bytes of unacknowledged data
int receiver_window = 65535; // Receiver's advertised window size

void init_window() {
    memset(send_window, 0, sizeof(send_window));
    for(int i = 0; i < WINDOW_SIZE; i++) {
        send_window[i].is_acked = 1;  // Mark as available
    }
}

// Check if packet timed out and needs retransmission
int check_timeout(struct timeval *sent_time) {
    struct timeval now, diff;
    gettimeofday(&now, NULL);
    timersub(&now, sent_time, &diff);
    return (diff.tv_sec * 1000 + diff.tv_usec / 1000) >= RTO_MS;
}

// Send data packet
void send_packet(int sockfd, window_slot *slot, struct sockaddr_in *servaddr, socklen_t len) {
    gettimeofday(&slot->sent_time, NULL);
    int is_retx = (slot->retransmissions > 0);
    int seq = slot->pkt.hdr.seq_num;
    int dlen = slot->pkt.hdr.data_size;
    
    // Simulate packet loss
    if (!should_drop_packet()) {
        sendto(sockfd, &slot->pkt, sizeof(packet), 0, (struct sockaddr *)servaddr, len);
        if (!chat_mode && LOG_ENABLED) {
            if (is_retx) log_write("RETX DATA SEQ=%d LEN=%d", seq, dlen);
            else log_write("SND DATA SEQ=%d LEN=%d", seq, dlen);
        }
        slot->retransmissions++;
    } else {
        if (!chat_mode && LOG_ENABLED) log_write("DROP DATA SEQ=%d", seq);
        // removed noisy stdout in file mode
        slot->retransmissions++;
    }
    // Note: bytes_in_flight is managed in the main loop, not here
}

// Message queue function implementations
void init_message_queue(message_queue *mq) {
    mq->front = 0;
    mq->rear = 0;
    mq->count = 0;
    mq->next_seq_to_send = 0;
}

int is_queue_full(message_queue *mq) {
    return mq->count >= 10;
}

int is_queue_empty(message_queue *mq) {
    return mq->count == 0;
}

void enqueue_message(message_queue *mq, const char *message) {
    if (is_queue_full(mq)) {
        printf("Message queue full! Message dropped.\n");
        return;
    }
    
    packet *pkt = &mq->messages[mq->rear];
    memset(pkt, 0, sizeof(packet));
    pkt->hdr.seq_num = mq->next_seq_to_send++;
    pkt->hdr.flags = DATA;
    pkt->hdr.data_size = strlen(message);
    memcpy(pkt->data, message, pkt->hdr.data_size);
    
    mq->rear = (mq->rear + 1) % 10;
    mq->count++;
}

int main(int argc, char *argv[]){
    parse_arguments(argc, argv);
    // Initialize verbose logging if requested (RUDP_LOG=1)
    log_init_client();
    
    chat_print("Connecting to %s:%d\n", server_ip, server_port);
    if (chat_mode) {
        chat_print("Chat mode enabled. Type '/quit' to exit.\n");
    } else {
        // File transfer mode: no extra stdout
    }
    if (loss_rate > 0.0) {
        chat_print("Packet loss rate: %.2f\n", loss_rate);
        srand(time(NULL));  // Initialize random seed for packet loss
    }
    
    int sockfd;
    header* client=(header*)malloc(sizeof(header));
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len = sizeof(servaddr);
    socklen_t send_len = sizeof(servaddr);

    // Create socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    // Fill server information
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = inet_addr(server_ip);
    servaddr.sin_port = htons(server_port);

    int cli_ori_seq = 1000;
    memset(client, 0, sizeof(*client));  // Initialize the whole struct to 0 first
    // Advertise chat intent in SYN when chat_mode is enabled
    client->flags = chat_mode ? (SYN | CHAT) : SYN;
    client->seq_num = cli_ori_seq;
    client->ack_num = 0;
    client->window_size = 65535;  // Maximum window size for basic flow control
    
    header* buffer = (header*)malloc(sizeof(header));
    memset(buffer, 0, sizeof(*buffer));

    int sent = sendto(sockfd, client, sizeof(*client), 0, (struct sockaddr *)&servaddr, send_len);
    if (LOG_ENABLED) {
        log_write("SND SYN SEQ=%u", client->seq_num);
    }
    if (sent < 0) {
        perror("sendto failed");
        exit(EXIT_FAILURE);
    }
    // removed extra stdout about sent bytes in all modes
    while(1){
        memset(buffer,0,sizeof(*buffer));
        int n = recvfrom(sockfd,buffer,sizeof(*buffer),0,(struct sockaddr *)&servaddr, &len);
        if (LOG_ENABLED && n > 0 && (buffer->flags & (SYN|ACK))==(SYN|ACK)) {
            log_write("RCV SYN-ACK SEQ=%u ACK=%u", buffer->seq_num, buffer->ack_num);
        }
        if(buffer->flags==(SYN|ACK) && buffer->ack_num==cli_ori_seq+1){
            // printf("3 way handshake for connection done\n");

            client->flags=ACK;
            client->ack_num=buffer->seq_num+1;

            sendto(sockfd,client,sizeof(*client),0,(struct sockaddr *)&servaddr, len);
            if (LOG_ENABLED) log_write("RCV ACK FOR SYN");
            
            if (chat_mode) {
                if (LOG_ENABLED) log_write("CHAT MODE START");
                int flags = fcntl(sockfd, F_GETFL, 0);
                fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK);
                
                fd_set readfds;
                char chat_buffer[PACKET_SIZE];
                packet chat_pkt;
                int chat_active = 1;
                int server_sent_fin = 0;

                // Chat message queue and transmission state
                message_queue mq;
                init_message_queue(&mq);
                
                // Current transmission state
                int awaiting_ack = 0;
                uint32_t last_sent_seq = 0;
                packet last_sent_pkt;
                struct timeval last_sent_time;
                
                // Duplicate detection for received messages
                uint32_t last_received_seq = UINT32_MAX;
                int prompt_shown = 0;

                while(chat_active) {
                    FD_ZERO(&readfds);
                    FD_SET(0, &readfds);           // stdin
                    FD_SET(sockfd, &readfds);      // socket

                    // Presentable prompt
                    if (!prompt_shown) {
                        // chat_print("Type your message: ");
                        prompt_shown = 1;
                    }

                    struct timeval timeout = {0, 100000};
                    int activity = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
                    if (activity < 0) { perror("select error"); break; }

                    // Keyboard input
                    if (FD_ISSET(0, &readfds)) {
                        if (fgets(chat_buffer, sizeof(chat_buffer), stdin) != NULL) {
                            // Do not reset prompt_shown here to avoid duplicate prompts
                            chat_buffer[strcspn(chat_buffer, "\n")] = 0;
                            if (strcmp(chat_buffer, "/quit") == 0) {
                                if (LOG_ENABLED) log_write("SND FIN SEQ=%u", cli_ori_seq + 2);
                                client->flags = FIN;
                                client->seq_num = cli_ori_seq + 2;
                                sendto(sockfd, client, sizeof(*client), 0, (struct sockaddr *)&servaddr, len);
                                chat_active = 0;
                                break;
                            }
                            // Show user's own message
                            chat_print("You: %s\n", chat_buffer);
                            // Add message to queue
                            enqueue_message(&mq, chat_buffer);
                        }
                    }

                    // Try to send next message from queue if not currently waiting for ACK
                    if (!awaiting_ack && !is_queue_empty(&mq)) {
                        packet *next_msg = &mq.messages[mq.front];
                        memcpy(&last_sent_pkt, next_msg, sizeof(packet));
                        last_sent_seq = next_msg->hdr.seq_num;
                        if (!should_drop_packet()) {
                            sendto(sockfd, next_msg, sizeof(packet), 0, (struct sockaddr *)&servaddr, len);
                            if (LOG_ENABLED) log_write("SND DATA SEQ=%u LEN=%u", next_msg->hdr.seq_num, next_msg->hdr.data_size);
                        } else {
                            if (LOG_ENABLED) log_write("DROP DATA SEQ=%u", next_msg->hdr.seq_num);
                        }
                        awaiting_ack = 1;
                        gettimeofday(&last_sent_time, NULL);
                    }

                    // Incoming messages
                    if (FD_ISSET(sockfd, &readfds)) {
                        memset(&chat_pkt, 0, sizeof(packet));
                        int n = recvfrom(sockfd, &chat_pkt, sizeof(packet), 0, 
                                        (struct sockaddr *)&servaddr, &len);
                        if (n > 0) {
                            if (chat_pkt.hdr.flags == DATA) {
                                if (LOG_ENABLED) log_write("RCV DATA SEQ=%u LEN=%u", chat_pkt.hdr.seq_num, chat_pkt.hdr.data_size);
                                if (chat_pkt.hdr.seq_num != last_received_seq) {
                                    chat_pkt.data[chat_pkt.hdr.data_size] = '\0';
                                    chat_print("\nServer: %s\n", chat_pkt.data);
                                    last_received_seq = chat_pkt.hdr.seq_num;
                                    prompt_shown = 0; // cause prompt to show again
                                }
                                packet ack_pkt;
                                memset(&ack_pkt, 0, sizeof(packet));
                                ack_pkt.hdr.flags = ACK;
                                ack_pkt.hdr.ack_num = chat_pkt.hdr.seq_num;
                                if (!should_drop_packet()) {
                                    sendto(sockfd, &ack_pkt, sizeof(packet), 0, 
                                          (struct sockaddr *)&servaddr, len);
                                    if (LOG_ENABLED) log_write("SND ACK=%u", ack_pkt.hdr.ack_num);
                                } else {
                                    if (LOG_ENABLED) log_write("DROP DATA SEQ=%u", ack_pkt.hdr.ack_num);
                                }
                            } else if (chat_pkt.hdr.flags == ACK) {
                                if (awaiting_ack && chat_pkt.hdr.ack_num == last_sent_seq) {
                                    if (LOG_ENABLED) log_write("RCV ACK=%u", last_sent_seq);
                                    awaiting_ack = 0;
                                    mq.front = (mq.front + 1) % 10;
                                    mq.count--;
                                    prompt_shown = 0; // show prompt again
                                }
                            } else if (chat_pkt.hdr.flags == FIN) {
                                if (LOG_ENABLED) log_write("RCV FIN SEQ=%u", chat_pkt.hdr.seq_num);
                                client->flags = ACK;
                                client->ack_num = chat_pkt.hdr.seq_num + 1;
                                sendto(sockfd, client, sizeof(*client), 0, (struct sockaddr *)&servaddr, len);
                                if (LOG_ENABLED) log_write("SND ACK FOR FIN");
                                client->flags = FIN;
                                client->seq_num = cli_ori_seq + 2;
                                sendto(sockfd, client, sizeof(*client), 0, (struct sockaddr *)&servaddr, len);
                                if (LOG_ENABLED) log_write("SND FIN SEQ=%u", client->seq_num);
                                server_sent_fin = 1;
                                chat_active = 0;
                                break;
                            }
                        }
                    }

                    // Retransmit if timeout and still awaiting ACK
                    if (awaiting_ack) {
                        struct timeval now, diff;
                        gettimeofday(&now, NULL);
                        timersub(&now, &last_sent_time, &diff);
                        long elapsed_ms = diff.tv_sec * 1000 + diff.tv_usec / 1000;
                        if (elapsed_ms >= RTO_MS) {
                            if (LOG_ENABLED) log_write("TIMEOUT SEQ=%u", last_sent_seq);
                            sendto(sockfd, &last_sent_pkt, sizeof(packet), 0, (struct sockaddr *)&servaddr, len);
                            if (LOG_ENABLED) log_write("RETX DATA SEQ=%u LEN=%u", last_sent_seq, last_sent_pkt.hdr.data_size);
                            gettimeofday(&last_sent_time, NULL);
                        }
                    }
                }
                // If server sent FIN, wait for final ACK and exit
                if (server_sent_fin) {
                    struct timeval timeout;
                    timeout.tv_sec = 2;
                    timeout.tv_usec = 0;
                    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
                    int termination_attempts = 0;
                    const int max_attempts = 50;
                    while (termination_attempts < max_attempts) {
                        memset(buffer, 0, sizeof(*buffer));
                        int n = recvfrom(sockfd, buffer, sizeof(*buffer), 0, (struct sockaddr *)&servaddr, &len);
                        if (n > 0 && buffer->flags == ACK && buffer->ack_num == cli_ori_seq + 3) {
                            if (LOG_ENABLED) log_write("RCV ACK=%u", buffer->ack_num);
                            // close and exit silently
                            close(sockfd);
                            free(client);
                            free(buffer);
                            return 0;
                        }
                        termination_attempts++;
                    }
                    if (LOG_ENABLED) log_write("Termination sequence failed after %d attempts, forcing exit", max_attempts);
                    close(sockfd);
                    free(client);
                    free(buffer);
                    return 0;
                }
            } else {
                // File transfer mode (suppress stdout; keep logs)
                // Initialize sliding window
                init_window();
                // Open input file for reading
                int fd = open(input_file, O_RDONLY);
                if (fd < 0) { perror("Failed to open input file"); exit(EXIT_FAILURE); }
                // Set socket to non-blocking for timeout handling
                fcntl(sockfd, F_SETFL, O_NONBLOCK);
                // First send the output filename to server
                packet filename_pkt; memset(&filename_pkt, 0, sizeof(packet));
                filename_pkt.hdr.seq_num = 0; filename_pkt.hdr.flags = DATA;
                filename_pkt.hdr.data_size = strlen(output_file);
                memcpy(filename_pkt.data, output_file, filename_pkt.hdr.data_size);
                int filename_acked = 0;
                while(!filename_acked) {
                    sendto(sockfd, &filename_pkt, sizeof(packet), 0, (struct sockaddr *)&servaddr, len);
                    // quiet stdout; optional logging only
                    packet ack_pkt; memset(&ack_pkt, 0, sizeof(packet));
                    int n = recvfrom(sockfd, &ack_pkt, sizeof(packet), 0, (struct sockaddr *)&servaddr, &len);
                    if(n > 0 && ack_pkt.hdr.flags == ACK && ack_pkt.hdr.ack_num == 0) {
                        filename_acked = 1;
                    }
                    usleep(100000);
                }
                next_seq = 1;  // Start file data from sequence 1
                while(1) {
                    // Fill window
                    while(window_used < WINDOW_SIZE) {
                        int slot_index = (base + window_used) % WINDOW_SIZE;
                        if(send_window[slot_index].is_acked) {
                            packet *pkt = &send_window[slot_index].pkt;
                            int bytes_read = read(fd, pkt->data, PACKET_SIZE);
                            if(bytes_read <= 0) { if(window_used == 0) goto transfer_complete; break; }
                            if(bytes_in_flight + bytes_read > receiver_window) {
                                if (!chat_mode && LOG_ENABLED) log_write("FLOW WIN UPDATE=%d", receiver_window);
                                lseek(fd, -bytes_read, SEEK_CUR);
                                break;
                            }
                            pkt->hdr.seq_num = next_seq++;
                            pkt->hdr.flags = DATA;
                            pkt->hdr.data_size = bytes_read;
                            send_window[slot_index].is_acked = 0;
                            send_window[slot_index].retransmissions = 0;
                            send_packet(sockfd, &send_window[slot_index], &servaddr, len);
                            window_used++;
                            bytes_in_flight += bytes_read;
                        } else { break; }
                    }
                    // Check for ACKs
                    packet recv_pkt; memset(&recv_pkt, 0, sizeof(recv_pkt));
                    int n = recvfrom(sockfd, &recv_pkt, sizeof(recv_pkt), 0, (struct sockaddr *)&servaddr, &len);
                    if(n > 0 && recv_pkt.hdr.flags == ACK) {
                        receiver_window = recv_pkt.hdr.window_size;
                        if (!chat_mode && LOG_ENABLED) {
                            log_write("RCV ACK=%d", recv_pkt.hdr.ack_num);
                            log_write("FLOW WIN UPDATE=%d", receiver_window);
                        }
                        while(base <= recv_pkt.hdr.ack_num && window_used > 0) {
                            int slot_index = base % WINDOW_SIZE;
                            if(!send_window[slot_index].is_acked) {
                                send_window[slot_index].is_acked = 1;
                                bytes_in_flight -= send_window[slot_index].pkt.hdr.data_size;
                                window_used--;
                            }
                            base++;
                        }
                    }
                    // Timeouts
                    for(int i = 0; i < window_used; i++) {
                        int slot_index = (base + i) % WINDOW_SIZE;
                        if(!send_window[slot_index].is_acked && check_timeout(&send_window[slot_index].sent_time)) {
                            if (!chat_mode && LOG_ENABLED) log_write("TIMEOUT SEQ=%d", send_window[slot_index].pkt.hdr.seq_num);
                            send_packet(sockfd, &send_window[slot_index], &servaddr, len);
                        }
                    }
                    usleep(1000);
                }
transfer_complete:
                close(fd);
                // Initiate termination quietly
                int flags = fcntl(sockfd, F_GETFL, 0);
                fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK);
                client->flags = FIN;
                client->seq_num = cli_ori_seq + 2;
                sendto(sockfd, client, sizeof(*client), 0, (struct sockaddr *)&servaddr, len);
            }
            // Handle termination sequence for both modes (quiet)
            {
                int got_ack = 0;
                int termination_attempts = 0;
                const int max_attempts = 5;
                struct timeval timeout;
                timeout.tv_sec = 2;  // 2 second timeout
                timeout.tv_usec = 0;
                setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
                
                while (termination_attempts < max_attempts) {
                    memset(buffer, 0, sizeof(*buffer));
                    int n = recvfrom(sockfd, buffer, sizeof(*buffer), 0, (struct sockaddr *)&servaddr, &len);
                    if (n < 0) {
                        termination_attempts++;
                        if (!got_ack) {
                            client->flags = FIN;
                            client->seq_num = cli_ori_seq + 2;
                            sendto(sockfd, client, sizeof(*client), 0, (struct sockaddr *)&servaddr, len);
                            if (LOG_ENABLED) log_write("SND FIN SEQ=%u (retransmit)", client->seq_num);
                        }
                        continue;
                    }
                    
                    if (buffer->flags == ACK) {
                        if (!got_ack) {
                            if (LOG_ENABLED) log_write("RCV ACK FOR FIN");
                            got_ack = 1;
                        }
                        continue;
                    }
                    
                    if (buffer->flags == FIN) {
                        if (LOG_ENABLED) log_write("RCV FIN SEQ=%u", buffer->seq_num);
                        client->flags = ACK;
                        client->ack_num = buffer->seq_num + 1;
                        sendto(sockfd, client, sizeof(*client), 0, (struct sockaddr *)&servaddr, len);
                        if (LOG_ENABLED) log_write("SND ACK FOR FIN");
                        if (LOG_FP) { fclose(LOG_FP); LOG_FP = NULL; }
                        close(sockfd);
                        free(client);
                        free(buffer);
                        return 0;
                    }
                }
                if (LOG_ENABLED) log_write("Termination sequence failed after %d attempts, forcing exit", max_attempts);
                if (LOG_FP) { fclose(LOG_FP); LOG_FP = NULL; }
                close(sockfd);
                free(client);
                free(buffer);
                return 0;
            }
        }
    }
}//llm code ends//