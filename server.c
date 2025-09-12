#include "sham.h"
// Add missing standard/system headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <openssl/evp.h>
#include <openssl/opensslv.h>
#include <stdarg.h>

#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define EVP_MD_CTX_new EVP_MD_CTX_create
#define EVP_MD_CTX_free EVP_MD_CTX_destroy
#endif

#define SYN 0x01
#define ACK 0x02
#define FIN 0x04

// Global variables for server configuration
int chat_mode = 0;
float loss_rate = 0.0;
static char last_received_filename[256] = ""; // for post-close MD5
static int file_transfer_completed = 0;        // flag indicating file rx done

// Verbose logging (file transfer mode only)
static int LOG_ENABLED = 0;
static FILE *LOG_FP = NULL;

// Simulate packet loss
int should_drop_packet() {
    if (loss_rate == 0.0) return 0;
    return (rand() / (float)RAND_MAX) < loss_rate;
}
//llm code starts//

void print_usage(char *program_name) {
    printf("Usage: %s <port> [--chat] [loss_rate]\n", program_name);
    printf("  port: Port number to bind to\n");
    printf("  --chat: Optional flag to enable chat mode\n");
    printf("  loss_rate: Optional packet loss rate (0.0-1.0), default 0.0\n");
    exit(EXIT_FAILURE);
}

int parse_arguments(int argc, char *argv[], int *port) {
    if (argc < 2) {
        print_usage(argv[0]);
    }
    
    // Parse port number
    *port = atoi(argv[1]);
    if (*port <= 0 || *port > 65535) {
        printf("Error: Invalid port number\n");
        exit(EXIT_FAILURE);
    }
    
    // Parse optional arguments
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--chat") == 0) {
            chat_mode = 1;
        } else {
            // Try to parse as loss rate
            loss_rate = atof(argv[i]);
            if (loss_rate < 0.0 || loss_rate > 1.0) {
                printf("Error: Loss rate must be between 0.0 and 1.0\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    
    return 0;
}

// Initialize receive buffer
recv_buffer* init_recv_buffer(int capacity) {
    recv_buffer* rb = (recv_buffer*)malloc(sizeof(recv_buffer));
    rb->packets = (packet*)malloc(capacity * sizeof(packet));
    rb->received = (int*)calloc(capacity, sizeof(int));
    rb->capacity = capacity;
    rb->base = 0;
    rb->next_seq = 0;
    rb->bytes_buffered = 0;
    rb->max_buffer_size = BUFFER_SIZE;
    return rb;
}

// Free receive buffer
void free_recv_buffer(recv_buffer* rb) {
    free(rb->packets);
    free(rb->received);
    free(rb);
}
//llm code ends//

//llm generated code starts//
// Calculate MD5 checksum of a file
void calculate_md5(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("Error: Could not open file %s for MD5 calculation\n", filename);
        return;
    }

#if defined(LIBRESSL_VERSION_NUMBER) || OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_MD_CTX ctx;
    EVP_MD_CTX_init(&ctx);
    if (EVP_DigestInit_ex(&ctx, EVP_md5(), NULL) != 1) {
        printf("Error: Could not initialize MD5 digest\n");
        EVP_MD_CTX_cleanup(&ctx);
        fclose(file);
        return;
    }
#else
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        printf("Error: Could not create MD5 context\n");
        fclose(file);
        return;
    }
    if (EVP_DigestInit_ex(ctx, EVP_md5(), NULL) != 1) {
        printf("Error: Could not initialize MD5 digest\n");
        EVP_MD_CTX_free(ctx);
        fclose(file);
        return;
    }
#endif

    unsigned char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), file)) > 0) {
#if defined(LIBRESSL_VERSION_NUMBER) || OPENSSL_VERSION_NUMBER < 0x10100000L
        if (EVP_DigestUpdate(&ctx, buf, n) != 1) {
            printf("Error: Could not update MD5 digest\n");
            EVP_MD_CTX_cleanup(&ctx);
            fclose(file);
            return;
        }
#else
        if (EVP_DigestUpdate(ctx, buf, n) != 1) {
            printf("Error: Could not update MD5 digest\n");
            EVP_MD_CTX_free(ctx);
            fclose(file);
            return;
        }
#endif
    }
    fclose(file);

    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int mdlen = 0;
#if defined(LIBRESSL_VERSION_NUMBER) || OPENSSL_VERSION_NUMBER < 0x10100000L
    if (EVP_DigestFinal_ex(&ctx, md, &mdlen) != 1) {
        printf("Error: Could not finalize MD5 digest\n");
        EVP_MD_CTX_cleanup(&ctx);
        return;
    }
    EVP_MD_CTX_cleanup(&ctx);
#else
    if (EVP_DigestFinal_ex(ctx, md, &mdlen) != 1) {
        printf("Error: Could not finalize MD5 digest\n");
        EVP_MD_CTX_free(ctx);
        return;
    }
    EVP_MD_CTX_free(ctx);
#endif

    // Print MD5 in the required format
    printf("MD5: ");
    for (unsigned int i = 0; i < mdlen; i++) printf("%02x", md[i]);
    printf("\n");
}

//llm generated code ends//

//llm geenrated code starts//
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

//llm generated code ends//

static void log_init_server() {
    // Always enable logging
    LOG_FP = fopen("server_log.txt", "w");
    if (LOG_FP) LOG_ENABLED = 1;
}
//llm code starts//

static void log_write(const char *fmt, ...) {
    // Return immediately if logging is disabled or the file pointer is invalid.
    if (!LOG_ENABLED || !LOG_FP) return;

    // 1. Get current time with microsecond precision.
    struct timeval tv;
    gettimeofday(&tv, NULL);

    // 2. Convert the seconds part to a broken-down time structure.
    struct tm tmv;
    localtime_r(&tv.tv_sec, &tmv); // localtime_r is a thread-safe version

    // 3. Format the timestamp string.
    char ts[64];
    snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d.%06ld",
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec, (long)tv.tv_usec);

    // 4. Print the timestamp prefix.
    fprintf(LOG_FP, "[%s] [LOG] ", ts);

    // 5. Handle the variadic arguments and print the actual log message.
    va_list ap;
    va_start(ap, fmt);
    vfprintf(LOG_FP, fmt, ap);
    va_end(ap);

    // 6. Add a newline and flush the buffer to ensure the log is written immediately.
    fputc('\n', LOG_FP);
    fflush(LOG_FP);
}
//llm code ends//

// Only print to stdout in chat mode; file mode should only print MD5 line
static void infof(const char *fmt, ...) {
    if (!chat_mode) return;
    va_list ap; va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

int main(int argc, char *argv[]){
    int port;
    parse_arguments(argc, argv, &port);
    log_init_server();
    
    infof("Starting server on port %d\n", port);
    if (chat_mode) {
        infof("Chat mode enabled\n");
    } else {
        // suppress file-mode stdout chatter
    }
    if (loss_rate > 0.0) {
        infof("Packet loss rate: %.2f\n", loss_rate);
        srand(time(NULL));  // Initialize random seed for packet loss
    }
    
    int sockfd;
    struct sockaddr_in cliaddr,serveraddr;
    header* server=(header*)malloc(sizeof(header));
    memset(server, 0, sizeof(*server));  // Initialize server header
    socklen_t len=sizeof(cliaddr);
    int n = 0; // reusable recvfrom return value across scopes
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&cliaddr,0,sizeof(cliaddr));
    memset(&serveraddr,0,sizeof(serveraddr));

    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = INADDR_ANY; 
    serveraddr.sin_port = htons(port);

    // Bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    header* buffer=(header*)malloc(sizeof(header));
    memset(buffer, 0, sizeof(*buffer));  // Initialize buffer
    int server_se_num=2000;
    
    infof("Server is ready and waiting for clients...\n");
    
    while(1){
        infof("Listening for new connection...\n");
        //llm generated code starts//


        // Handle SYN packet
        while(1) {
            memset(buffer,0,sizeof(*buffer));
            int n = recvfrom(sockfd, buffer, sizeof(*buffer), 0, (struct sockaddr *)&cliaddr, &len);
            if (n < 0) {
                perror("recvfrom failed");
                continue;
            }
            infof("Received %d bytes from client\n", n);
            
            if (buffer->flags & SYN) {  // accept SYN with possible extra bits (e.g., CHAT)
                if (LOG_ENABLED) log_write("RCV SYN SEQ=%u", buffer->seq_num);
                server->flags=(SYN|ACK);
                server->seq_num=server_se_num;
                server->ack_num=buffer->seq_num+1;
                server->window_size=65535;  // Maximum window size for basic flow control

                if (LOG_ENABLED) log_write("SND SYN-ACK SEQ=%u ACK=%u", server->seq_num, server->ack_num);
                sendto(sockfd,server,sizeof(*server),0,(struct sockaddr *)&cliaddr, len);
                break; // Exit SYN handling loop
            }
        }
        
        //llm generated code ends//


        // Handle ACK packet
        while(1) {
            memset(buffer,0,sizeof(*buffer));
            int n = recvfrom(sockfd, buffer, sizeof(*buffer), 0, (struct sockaddr *)&cliaddr, &len);
            if (n < 0) {
                continue;
            }
            
            if(buffer->flags==ACK){
                if(buffer->ack_num==server_se_num+1){
                    if (!chat_mode && LOG_ENABLED) log_write("RCV ACK FOR SYN");
                    infof("Completed handshake, ready for data transfer\n");
                    
                    //llm code starts//
                    // Only use chat_mode from server's command line
                    if (chat_mode) {
                        // infof("Entering chat mode for this session\n");
                        int flags = fcntl(sockfd, F_GETFL, 0);
                        fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK);

                        fd_set readfds;
                        char chat_buffer[PACKET_SIZE];
                        packet chat_pkt;
                        int active_close = 0;
                        int got_fin_ack = 0;

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

                        while(1) {
                            FD_ZERO(&readfds);
                            FD_SET(0, &readfds);           // stdin
                            FD_SET(sockfd, &readfds);      // socket

                            // Removed chat prompt to minimize terminal output
                            // if (!active_close && !prompt_shown) { printf("Type your message: "); fflush(stdout); prompt_shown = 1; }

                            struct timeval timeout = {0, 100000}; // 100ms timeout for responsiveness
                            int activity = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

                            if (activity < 0) {
                                perror("select error");
                                break;
                            }

                            // Keyboard input - accept input without prints
                            if (FD_ISSET(0, &readfds) && !active_close) {
                                if (fgets(chat_buffer, sizeof(chat_buffer), stdin) != NULL) {
                                    prompt_shown = 0;
                                    chat_buffer[strcspn(chat_buffer, "\n")] = 0;
                                    
                                    if (strcmp(chat_buffer, "/quit") == 0) {
                                        // printf("Initiating disconnect...\n");
                                        memset(server, 0, sizeof(*server));
                                        server->flags = FIN;
                                        server->seq_num = server_se_num + 2;
                                        sendto(sockfd, server, sizeof(*server), 0, 
                                              (struct sockaddr *)&cliaddr, len);
                                        // printf("Sent FIN to client (active close)\n");
                                        log_write("Sent FIN (active close): seq=%d", server->seq_num);
                                        active_close = 1;
                                        got_fin_ack = 0;
                                        continue;
                                    }
                                    
                                    // Add message to queue (no terminal print)
                                    enqueue_message(&mq, chat_buffer);
                                    // printf("You: %s\n", chat_buffer);
                                }
                            }

                            // Try to send next message from queue if not currently waiting for ACK
                            if (!awaiting_ack && !is_queue_empty(&mq)) {
                                packet *next_msg = &mq.messages[mq.front];
                                memcpy(&last_sent_pkt, next_msg, sizeof(packet));
                                last_sent_seq = next_msg->hdr.seq_num;
                                
                                // Always echo what we try to send
                                printf("You: %.*s\n", next_msg->hdr.data_size, next_msg->data);
                                fflush(stdout);
                                if (!should_drop_packet()) {
                                    sendto(sockfd, next_msg, sizeof(packet), 0, (struct sockaddr *)&cliaddr, len);
                                    log_write("Sent message: seq=%u, data=\"%.*s\"", 
                                              next_msg->hdr.seq_num, 
                                              next_msg->hdr.data_size, next_msg->data);
                                } else {
                                    if (LOG_ENABLED) log_write("DROP DATA SEQ=%u", next_msg->hdr.seq_num);
                                }
                                
                                awaiting_ack = 1;
                                gettimeofday(&last_sent_time, NULL);
                            }

                            // Incoming messages
                            if (FD_ISSET(sockfd, &readfds)) {
                                memset(&chat_pkt, 0, sizeof(packet));
                                n = recvfrom(sockfd, &chat_pkt, sizeof(packet), 0, 
                                            (struct sockaddr *)&cliaddr, &len);
                                if (n > 0) {
                                    header *h = (header*)&chat_pkt;
                                    
                                    if (active_close && h->flags == ACK && !got_fin_ack) {
                                        if (h->ack_num == (server_se_num + 2) + 1) {
                                            got_fin_ack = 1;
                                            // printf("Client ACKed our FIN\n");
                                            log_write("Received ACK for FIN: ack=%d", h->ack_num);
                                            continue;
                                        }
                                    }
                                    if (h->flags == FIN) {
                                        // FIN handling (logs only)
                                        if (active_close) {
                                            if (LOG_ENABLED) log_write("RCV FIN SEQ=%u", h->seq_num);
                                            header final_ack = {0};
                                            final_ack.flags = ACK;
                                            final_ack.ack_num = h->seq_num + 1;
                                            sendto(sockfd, &final_ack, sizeof(final_ack), 0,
                                                  (struct sockaddr *)&cliaddr, len);
                                            if (LOG_ENABLED) log_write("SND ACK FOR FIN");
                                            server_se_num += 1000;
                                            goto server_exit;
                                        } else {
                                            goto handle_fin;
                                        }
                                    }
                                    if (h->flags == DATA) {
                                        // Receiver-side simulated drop for chat data
                                        if (should_drop_packet()) {
                                            if (LOG_ENABLED) log_write("DROP DATA SEQ=%u", chat_pkt.hdr.seq_num);
                                            continue;
                                        }
                                        // Print received client message
                                        if (chat_pkt.hdr.seq_num != last_received_seq) {
                                            chat_pkt.data[chat_pkt.hdr.data_size] = '\0';
                                            printf("\nClient: %s\n", chat_pkt.data);
                                            fflush(stdout);
                                            last_received_seq = chat_pkt.hdr.seq_num;
                                            prompt_shown = 0;
                                        }
                                        // Always send ACK regardless of duplicate
                                        packet ack_pkt;
                                        memset(&ack_pkt, 0, sizeof(packet));
                                        ack_pkt.hdr.flags = ACK;
                                        ack_pkt.hdr.ack_num = chat_pkt.hdr.seq_num;
                                        if (!should_drop_packet()) {
                                            sendto(sockfd, &ack_pkt, sizeof(packet), 0, 
                                                  (struct sockaddr *)&cliaddr, len);
                                            log_write("Sent ACK: seq=%u, ack=%u", 
                                                      ack_pkt.hdr.seq_num, ack_pkt.hdr.ack_num);
                                        } else {
                                            if (LOG_ENABLED) log_write("DROP ACK ACK=%u", ack_pkt.hdr.ack_num);
                                        }
                                    }
                                    if (h->flags == ACK) {
                                        if (awaiting_ack && h->ack_num == last_sent_seq) {
                                            // printf("ACK received for seq %u\n", last_sent_seq);
                                            awaiting_ack = 0;
                                            mq.front = (mq.front + 1) % 10;
                                            mq.count--;
                                            prompt_shown = 0;
                                        }
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
                                    sendto(sockfd, &last_sent_pkt, sizeof(packet), 0, (struct sockaddr *)&cliaddr, len);
                                    if (LOG_ENABLED) log_write("RETX DATA SEQ=%u LEN=%u", last_sent_seq, last_sent_pkt.hdr.data_size);
                                    gettimeofday(&last_sent_time, NULL);
                                }
                            }
                        }

chat_terminated:
                        // Chat mode ended, go back to main loop (server keeps running)
                        break; // Exit ACK handling loop to start new connection
                    } //llm code ends//
                    else {

                        //llm code starts//

                        // File transfer mode
                        fcntl(sockfd, F_SETFL, O_NONBLOCK);
                        
                        // First, receive the output filename
                        char output_filename[256] = "received_file.txt"; // default
                        packet filename_pkt;
                        if (LOG_ENABLED) log_write("Waiting for filename packet");
                        
                        while(1) {
                            memset(&filename_pkt, 0, sizeof(packet));
                            n = recvfrom(sockfd, &filename_pkt, sizeof(packet), 0, 
                                        (struct sockaddr *)&cliaddr, &len);
                            if (n > 0 && filename_pkt.hdr.flags == DATA) {
                                strncpy(output_filename, filename_pkt.data, filename_pkt.hdr.data_size);
                                output_filename[filename_pkt.hdr.data_size] = '\0';
                                strncpy(last_received_filename, output_filename, sizeof(last_received_filename)-1);
                                last_received_filename[sizeof(last_received_filename)-1] = '\0';
                                if (LOG_ENABLED) log_write("RCV DATA SEQ=%u LEN=%u", filename_pkt.hdr.seq_num, filename_pkt.hdr.data_size);
                                // Send ACK for filename
                                packet ack_pkt;
                                memset(&ack_pkt, 0, sizeof(packet));
                                ack_pkt.hdr.flags = ACK;
                                ack_pkt.hdr.ack_num = 0; // Special ACK for filename
                                ack_pkt.hdr.window_size = BUFFER_SIZE;
                                sendto(sockfd, &ack_pkt, sizeof(packet), 0, 
                                      (struct sockaddr *)&cliaddr, len);
                                if (LOG_ENABLED) log_write("SND ACK=%u WIN=%u", ack_pkt.hdr.ack_num, ack_pkt.hdr.window_size);
                                break;
                            }
                            usleep(10000); // 10ms delay
                        }
                        
                        recv_buffer* rb = init_recv_buffer(WINDOW_SIZE * 2);
                        int outfd = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        if (outfd < 0) { perror("Failed to create output file"); exit(EXIT_FAILURE); }
                        if (LOG_ENABLED) log_write("Starting file reception: %s", output_filename);
                        
                        // Data reception loop
                        packet data_pkt;
                        while(1) {
                            memset(&data_pkt, 0, sizeof(packet));
                            n = recvfrom(sockfd, &data_pkt, sizeof(packet), 0, 
                                        (struct sockaddr *)&cliaddr, &len);
                            
                            if(n > 0) {
                                if(data_pkt.hdr.flags == FIN) {
                                    if (LOG_ENABLED) log_write("RCV FIN SEQ=%u", data_pkt.hdr.seq_num);
                                    break;  // End of transmission
                                }
                                
                                if(data_pkt.hdr.flags == DATA) {
                                    uint32_t seq = data_pkt.hdr.seq_num;
                                    if (seq == 0) continue; // skip filename packet if resent
                                    // Receiver-side simulated drop for file data
                                    if (should_drop_packet()) {
                                        if (!chat_mode && LOG_ENABLED) log_write("DROP DATA SEQ=%u", seq);
                                        continue; // drop without ACK
                                    }
                                    if (LOG_ENABLED) log_write("RCV DATA SEQ=%u LEN=%u", seq, data_pkt.hdr.data_size);
                                    // In-order delivery and buffering
                                    uint32_t file_seq = seq - 1;
                                    if(file_seq == rb->next_seq) {
                                        write(outfd, data_pkt.data, data_pkt.hdr.data_size);
                                        rb->next_seq++;
                                        while(rb->received[rb->next_seq % rb->capacity]) {
                                            packet *buffered = &rb->packets[rb->next_seq % rb->capacity];
                                            write(outfd, buffered->data, buffered->hdr.data_size);
                                            rb->received[rb->next_seq % rb->capacity] = 0;
                                            rb->bytes_buffered -= buffered->hdr.data_size;
                                            rb->next_seq++;
                                        }
                                    } else if(file_seq > rb->next_seq) {
                                        int idx = file_seq % rb->capacity;
                                        if(rb->bytes_buffered + data_pkt.hdr.data_size <= rb->max_buffer_size) {
                                            memcpy(&rb->packets[idx], &data_pkt, sizeof(packet));
                                            rb->received[idx] = 1;
                                            rb->bytes_buffered += data_pkt.hdr.data_size;
                                        }
                                    }
                                    int available_buffer = rb->max_buffer_size - rb->bytes_buffered;
                                    if (LOG_ENABLED) log_write("FLOW WIN UPDATE=%d", available_buffer);
                                    packet ack_pkt; memset(&ack_pkt, 0, sizeof(packet));
                                    ack_pkt.hdr.flags = ACK;
                                    ack_pkt.hdr.ack_num = rb->next_seq; // next expected file seq
                                    ack_pkt.hdr.window_size = available_buffer;
                                    if (!should_drop_packet()) {
                                        sendto(sockfd, &ack_pkt, sizeof(packet), 0, (struct sockaddr *)&cliaddr, len);
                                        if (LOG_ENABLED) log_write("SND ACK=%u WIN=%u", ack_pkt.hdr.ack_num, ack_pkt.hdr.window_size);
                                    } else {
                                        if (LOG_ENABLED) log_write("DROP ACK ACK=%u", ack_pkt.hdr.ack_num);
                                    }
                                }
                            }
                            usleep(1000);
                        }
                        close(outfd);
                        free_recv_buffer(rb);
                        file_transfer_completed = 1;
                        
                        // Set socket back to blocking for termination sequence
                        int bflags = fcntl(sockfd, F_GETFL, 0);
                        fcntl(sockfd, F_SETFL, bflags & ~O_NONBLOCK);
                        
                        // Now wait for FIN packet to begin 4-way close
                        while(1) {
                            memset(buffer,0,sizeof(*buffer));
                            n = recvfrom(sockfd, buffer, sizeof(*buffer), 0, (struct sockaddr *)&cliaddr, &len);
                            if (n > 0 && buffer->flags == FIN) {
                                goto handle_fin;
                            }
                            usleep(10000);
                        }

                        //llm code ends//

                    }
                }
            } else if (buffer->flags == FIN) {
                goto handle_fin;
            }
        }
//llm code starts//
handle_fin:
        if (LOG_ENABLED) log_write("RCV FIN SEQ=%u", buffer->seq_num);
        // infof("Received FIN from client\n");
        
        // Step 2: Send ACK for client's FIN
        server->flags = ACK;
        server->ack_num = buffer->seq_num + 1;
        if (LOG_ENABLED) log_write("SND ACK FOR FIN");
        sendto(sockfd,server,sizeof(*server),0,(struct sockaddr *)&cliaddr, len);
        // infof("Sent ACK for FIN\n");
        
        // Step 3: Send server's FIN
        server->flags = FIN;
        server->seq_num = server_se_num + 2;  // New sequence number
        if (LOG_ENABLED) log_write("SND FIN SEQ=%u", server->seq_num);
        sendto(sockfd,server,sizeof(*server),0,(struct sockaddr *)&cliaddr, len);
        // infof("Sent FIN to client\n");
        
        // Wait for final ACK from client (with timeout)
        int final_ack_attempts = 0;
        const int max_final_attempts = 3;
        
        while(final_ack_attempts < max_final_attempts){
            final_ack_attempts++;
            memset(buffer,0,sizeof(*buffer));
            
            // Set timeout for final ACK
            struct timeval timeout;
            timeout.tv_sec = 3;  // 3 second timeout
            timeout.tv_usec = 0;
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            
            n = recvfrom(sockfd,buffer,sizeof(*buffer),0,(struct sockaddr *)&cliaddr, &len);
            if(n < 0){
                // infof("Timeout waiting for final ACK (attempt %d/%d)\n", final_ack_attempts, max_final_attempts);
                continue;
            }
            if(buffer->flags==ACK && buffer->ack_num==server->seq_num+1){
                if (LOG_ENABLED) log_write("RCV ACK=%u", buffer->ack_num);
                // Print MD5 only after full close in file mode
                if (!chat_mode && file_transfer_completed && last_received_filename[0] != '\0') {
                    calculate_md5(last_received_filename);
                    file_transfer_completed = 0;
                    last_received_filename[0] = '\0';
                }
                // infof("Server ready for next client...\n");
                
                // Reset server sequence number for next client
                server_se_num += 1000;  // Increment for next client
                
                // Clear timeout setting
                timeout.tv_sec = 0;
                timeout.tv_usec = 0;
                setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
                goto server_exit; // terminate after successful close
            }
        }
        // If we are here, final ACK attempts exhausted; terminate anyway
        {
            struct timeval timeout; timeout.tv_sec = 0; timeout.tv_usec = 0;
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            goto server_exit;
        }

        //llm code ends//
    }

server_exit:
    // Clean up allocated memory before exiting
    free(server);
    free(buffer);

    if (LOG_FP) { fclose(LOG_FP); }
    return 0;
}