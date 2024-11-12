#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT1 2201
#define PORT2 2202
#define BUFFER_SIZE 1024
#define MISS -1
#define HIT -2

// Define a structure to hold data about a ship
typedef struct {
    int type;
    int rotation;
    int row;
    int col;
} Ship ;

// W x H, Initialized by P1
int board_width = 0;
int board_height = 0;

// Initalize each player's board
int ** p1_board;
int ** p2_board;

int p1_ships_remaining = 5;
int p2_ships_remaining = 5;

long file_size = 0;

char * load_file(const char * filename) {
    char *buffer = 0;
    long length;
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        return NULL;
    }
    
    (void)fseek(file, 0, SEEK_END);
    length = ftell(file);
    (void)fseek(file, 0, SEEK_SET);
    buffer = (char *)malloc(length + 1);

    if (buffer != NULL) {
        (void)fread(buffer, 1, length, file);
        (void)fclose(file);
    }

    file_size = length;
    return buffer;
}

int create_socket(int port) {
    int sockfd; // Socket file descriptor
    struct sockaddr_in server_addr; // Holds server address + information

    // AF_INET specifies address family (32-bit IPv4)
    // SOCK_STREAM indicates that it is a TCP socket
    // 0 is the default protocol for the given socket type

    //Creates a socket and checks for failure
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
       perror("Failed to create socket");
    }

    int opt=1;

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))");
        //exit(EXIT_FAILURE);
    }
    if (setsockopt(sockfd, SOL_SOCKET, 2, &opt, sizeof(opt))) {
        perror("setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))");
        //exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(sockfd,(struct sockaddr *) & server_addr, sizeof(server_addr)) < 0){
        perror("Failed to bind");
    }

    if (listen(sockfd, 3) < 0){
        perror("Failed to listen");
    }

    printf("Server running on port %d......\n", port);
    return sockfd;
}

void free_board(int **board, int board_height) {
    if (board != NULL) {
        for (int i = 0; i < board_height; i++) {
            free(board[i]);  // Free each row
        }
        free(board);  // Finally free the pointer to the array of rows
    }
}

// Read a message from client => server
int read_message(int sockfd, char * buffer, int buffer_size) {
    int bytes_read = read(sockfd, buffer, buffer_size - 1);
    buffer[bytes_read] = '\0';
    if (bytes_read < 0) {
        perror("Failed to read message.");
        exit(1);
    }
    return bytes_read;
} 

// Write a message from server => client
void write_message(int sockfd, const char * message) {
    if (write(sockfd, message, strlen(message)) < 0) {
        perror("Failed to write message.");
    }
}

// Halt Reponse to a Forfeit Packet
void halt_response(int sockfd, int other_sockfd) {
    char buffer[BUFFER_SIZE];
    write_message(sockfd, "H 0"); // Send a (loss) Halt Packet to P1
    read_message(other_sockfd, buffer, sizeof(buffer)); // Read from the other socket without storing it
    write_message(other_sockfd, "H 1"); // Send a (win) Halt Packet to P2

    free_board(p1_board, board_height);
    free_board(p2_board, board_height);
    // Close the connection to the ports after sending the Halt packets
    close(sockfd);  // Close Player A's connection
    close(other_sockfd);  // Close Player B's connection
}

void acknowledgement_response(int sockfd) {
    write_message(sockfd, "A");
}

void error_response(int sockfd, int error_code) {
    char message[BUFFER_SIZE];  // Buffer to hold the error message
    snprintf(message, sizeof(message), "E %d", error_code); // Format the error message, including the error code
    write_message(sockfd, message);
}

int process_begin_packet(int other_client_sockfd, int client_sockfd, int player) {
    char buffer[BUFFER_SIZE];
    int dummy = 0;
    while (1) {
        if (player == 1) {
            printf("Enter the Begin packet (player 1): \n");
            int bytes_read = read_message(client_sockfd, buffer, sizeof(buffer));

            if (strncmp(buffer, "F", 1) == 0) { // Forfeit
                halt_response(client_sockfd, other_client_sockfd); // Send Halt response packets, close connections
                return 0;
            } else if (strncmp(buffer, "B", 1) == 0) { // Begin packet
                if ((sscanf(buffer, "B %d %d %d", &board_width, &board_height, &dummy) == 2) && (board_width >= 10) && (board_height >= 10)) {
                    
                    p1_board = (int **)malloc(board_height * sizeof(int *));
                    for (int i = 0; i < board_height; i++) {
                        p1_board[i] = (int *)malloc(board_width * sizeof(int));
                    }
                    p2_board = (int **)malloc(board_height * sizeof(int *));
                    for (int i = 0; i < board_height; i++) {
                        p2_board[i] = (int *)malloc(board_width * sizeof(int));
                    }

                    // Initialize the boards
                    for (int i = 0; i < board_height; i++) {
                        for (int j = 0; j < board_width; j++) {
                            p1_board[i][j] = 0;
                            p2_board[i][j] = 0;
                        }
                    }
                    acknowledgement_response(client_sockfd); // Acknowledge the client's message
                    return 1; 
                } else {
                    error_response(client_sockfd, 200); // Invalid Begin Packet (invalid number of parameters)
                }
            } else {
                error_response(client_sockfd, 100); // Invalid Packet Type (expected begin packet)
            }
        } else if (player == 2) {
            printf("Enter the Begin packet (player 2): \n");
            int bytes_read = read_message(client_sockfd, buffer, sizeof(buffer));
            printf("%s", buffer);
            if (bytes_read < 0) {
                perror("Error reading from client."); 
                return 0; 
            }

            if (strncmp(buffer, "F", 1) == 0) { // Forfeit
                halt_response(client_sockfd, other_client_sockfd); // Send Halt response packets, close connections
                return 0;
            } else if (strncmp(buffer, "B", 1) == 0) {
                if (sscanf(buffer, "B %d", &dummy) == -1) {
                    acknowledgement_response(client_sockfd); // Acknowledge the client's message
                    return 1;
                } else {
                    error_response(client_sockfd, 200);
                }
            } else {
                error_response(client_sockfd, 100); // Invalid Packet Type (expected begin packet)
            }
        } else {
            perror("You entered an invalid player argument. Try again.");
        }
    }
}

bool check_within_border(int type, int rotation, int row, int col) {
    bool within_border = true;
   
    if (type == 1) { // Check ship type 1
        for (int r = row; r <= row + 1; r++) {
            if (r < 0 || r >= board_height) {
                within_border = false;
            }
        }
        for (int c = col; c <= col + 1; c++) {
            if (c < 0 || c >= board_width) {
                within_border = false;
            }
        }
    } else if (type == 2) { // Check ship type 2
        if (rotation == 1 || rotation == 3) {
            for (int r = row; r <= row + 3; r++) {
                if (r < 0 || r >= board_height) {
                    within_border = false;
                }
            }
            if (col < 0 || col >= board_width) {
                within_border = false;
            }
        }
        if (rotation == 2 || rotation == 4) {
            if (row < 0 || row >= board_height) {
                within_border = false;
            }
            for (int c = col; c <= col + 3; c++) {
                if (c < 0 || c >= board_width) {
                    within_border = false;
                }
            }
        }
    } else if (type == 3) { // Check ship type 3
        if (rotation == 1 || rotation == 3) {
            for (int r = row; r >= row - 1; r--) {
                if (r < 0 || r >= board_height) {
                    within_border = false;
                }
            }
            for (int c = col; c <= col + 2; c++) {
                if (c < 0 || c >= board_width) {
                    within_border = false;
                }
            }
        }
        if (rotation == 2 || rotation == 4) {
            for (int r = row; r <= row + 2; r++) {
                if (r < 0 || r >= board_height) {
                    within_border = false;
                }
            }
            for (int c = col; c <= col + 1; c++) {
                if (c < 0 || c >= board_width) {
                    within_border = false;
                }
            }
        }
    } else if (type == 4) { // Check ship type 4
        if (rotation == 1 || rotation == 3) {
            for (int r = row; r <= row + 2; r++) {
                if (r < 0 || r >= board_height) {
                    within_border = false;
                }
            }
            for (int c = col; c <= col + 1; c++) {
                if (c < 0 || c >= board_width) {
                    within_border = false;
                }
            }
        }
        if (rotation == 2) {
            for (int r = row; r <= row + 1; r++) {
                if (r < 0 || r >= board_height) {
                    within_border = false;
                }
            }
            for (int c = col; c <= col + 2; c++) {
                if (c < 0 || c >= board_width) {
                    within_border = false;
                }
            }
        }
        if (rotation == 4) {
            for (int r = row; r >= row - 1; r--) {
                if (r < 0 || r >= board_height) {
                    within_border = false;
                }
            }
            for (int c = col; c <= col + 2; c++) {
                if (c < 0 || c >= board_width) {
                    within_border = false;
                }
            }
        }
    } else if (type == 5) {
        if (rotation == 1 || rotation == 3) {
             for (int r = row; r <= row + 1; r++) {
                if (r < 0 || r >= board_height) {
                    within_border = false;
                }
            }
            for (int c = col; c <= col + 1; c++) {
                if (c < 0 || c >= board_width) {
                    within_border = false;
                }
            }
        }
        if (rotation == 2 || rotation == 4) {
            for (int r = row - 1; r <= row + 1; r++) {
                if (r < 0 || r >= board_height) {
                    within_border = false;
                }
            }
            for (int c = col; c <= col + 1; c++) {
                if (c < 0 || c >= board_width) {
                    within_border = false;
                }
            }
        }
    } else if (type == 6) {
        if (rotation == 2 || rotation == 4) {
            for (int r = row; r <= row + 1; r++) {
                if (r < 0 || r >= board_height) {
                    within_border = false;
                }
            }
            for (int c = col; c <= col + 2; c++) {
                if (c < 0 || c >= board_width) {
                    within_border = false;
                }
            }
        }
        if (rotation == 1) {
            for (int r = row; r >= row - 2; r--) {
                if (r < 0 || r >= board_height) {
                    within_border = false;
                }
            }
            for (int c = col; c <= col + 1; c++) {
                if (c < 0 || c >= board_width) {
                    within_border = false;
                }
            }
        }
        if (rotation == 3) {
            for (int r = row; r <= row + 2; r++) {
                if (r < 0 || r >= board_height) {
                    within_border = false;
                }
            }
            for (int c = col; c <= col + 1; c++) {
                if (c < 0 || c >= board_width) {
                    within_border = false;
                }
            }
        }
    
    } else {
        if (rotation == 1) {
            for (int r = row; r <= row + 1; r++) {
                if (r < 0 || r >= board_height) {
                    within_border = false;
                }
            }
            for (int c = col; c <= col + 2; c++) {
                if (c < 0 || c >= board_width) {
                    within_border = false;
                }
            }
        } else if (rotation == 2) {
            for (int r = row - 1; r <= row + 1; r++) {
                if (r < 0 || r >= board_height) {
                    within_border = false;
                }
            }
            for (int c = col; c <= col + 1; c++) {
                if (c < 0 || c >= board_width) {
                    within_border = false;
                }
            }
        } else if (rotation == 3) {
            for (int r = row; r >= row - 1; r--) {
                if (r < 0 || r >= board_height) {
                    within_border = false;
                }
            }
            for (int c = col; c <= col + 2; c++) {
                if (c < 0 || c >= board_width) {
                    within_border = false;
                }
            }
        } else {
            for (int r = row; r <= row + 2; r++) {
                if (r < 0 || r >= board_height) {
                    within_border = false;
                }
            }
            for (int c = col; c <= col + 1; c++) {
                if (c < 0 || c >= board_width) {
                    within_border = false;
                }
            }
        }
    }
    return within_border;
}

bool check_no_overlap(int type, int rotation, int row, int col, int ** board) {
    bool no_overlap = false;
    if (type == 1) {
        if (board[row][col] == 0 && board[row][col+1] == 0 && board[row+1][col] == 0 && board[row+1][col+1] == 0) {
            no_overlap = true;
        }
    } else if (type == 2) {
        if (rotation == 1 || rotation == 3) {
            if (board[row][col] == 0 && board[row+1][col] == 0 && board[row+2][col] == 0 && board[row+3][col] == 0) {
                no_overlap = true;
            }
        } else { // rotation == 2 || rotation == 4
            if (board[row][col] == 0 && board[row][col+1] == 0 && board[row][col+2] == 0 && board[row][col+3] == 0) {
                no_overlap = true;
            }
        }
    } else if (type == 3) {
        if (rotation == 1 || rotation == 3) {
            if (board[row][col] == 0 && board[row][col+1] == 0 && board[row-1][col+1] == 0 && board[row-1][col+2] == 0) {
                no_overlap = true;
            }
        } else { // rotation == 2 || rotation == 4
            if (board[row][col] == 0 && board[row+1][col] == 0 && board[row+1][col+1] == 0 && board[row+2][col+1] == 0) {
                no_overlap = true;
            }
        }
    } else if (type == 4) {
        if (rotation == 1) {
            if (board[row][col] == 0 && board[row+1][col] == 0 && board[row+2][col] == 0 && board[row+2][col+1] == 0) {
                no_overlap = true;
            }
        } else if (rotation == 2) {
            if (board[row+1][col] == 0 && board[row][col] == 0 && board[row][col+1] == 0 && board[row][col+2] == 0) {
                no_overlap = true;
            }
        } else if (rotation == 3) {
            if (board[row][col] == 0 && board[row][col+1] == 0 && board[row+1][col+1] == 0 && board[row+2][col+1] == 0) {
                no_overlap = true;
            }
        } else { // rotation == 4
            if (board[row][col] == 0 && board[row][col+1] == 0 && board[row][col+2] == 0 && board[row-1][col+2] == 0) {
                no_overlap = true;
            }
        }
    } else if (type == 5) {
        if (rotation == 1 || rotation == 3) {
            if (board[row][col] == 0 && board[row][col+1] == 0 && board[row+1][col+1] == 0 && board[row+1][col+2] == 0) {
                no_overlap = true;
            }
        } else { // rotation == 2 || rotation == 4
            if (board[row+1][col] == 0 && board[row][col] == 0 && board[row][col+1] == 0 && board[row-1][col+1] == 0) {
                no_overlap = true;
            }
        }
    } else if (type == 6) {
        if (rotation == 1) {
            if (board[row][col] == 0 && board[row][col+1] == 0 && board[row-1][col+1] == 0 && board[row-2][col+1] == 0) {
                no_overlap = true;
            }
        } else if (rotation == 2) {
            if (board[row][col] == 0 && board[row+1][col] == 0 && board[row+1][col+1] == 0 && board[row+1][col+2] == 0) {
                no_overlap = true;
            }
        } else if (rotation == 3) {
            if (board[row][col+1] == 0 && board[row][col] == 0 && board[row+1][col] == 0 && board[row+2][col] == 0) {
                no_overlap = true;
            }
        } else { // rotation == 4
            if (board[row][col] == 0 && board[row][col+1] == 0 && board[row][col+2] == 0 && board[row+1][col+2] == 0) {
                no_overlap = true;
            }
        }
    } else { // type == 7
        if (rotation == 1) {
            if (board[row][col] == 0 && board[row][col+1] == 0 && board[row][col+2] == 0 && board[row+1][col+1] == 0) {
                no_overlap = true;
            }
        } else if (rotation == 2) {
            if (board[row][col] == 0 && board[row-1][col+1] == 0 && board[row][col+1] == 0 && board[row+1][col+1] == 0) {
                no_overlap = true;
            }
        } else if (rotation == 3) {
            if (board[row][col] == 0 && board[row][col+1] == 0 && board[row][col+2] == 0 && board[row-1][col+1] == 0) {
                no_overlap = true;
            }
        } else { // rotation == 4
            if (board[row][col] == 0 && board[row+1][col] == 0 && board[row+2][col] == 0 && board[row+1][col+1] == 0) {
                no_overlap = true;
            }
        }
    }
    return no_overlap;
}

void place_ship(int type, int rotation, int row, int col, int ** board) {
    if (type == 1) {
        board[row][col] = 1;
        board[row][col+1] = 1;
        board[row+1][col] = 1;
        board[row+1][col+1] = 1;
    } else if (type == 2) {
        if (rotation == 1 || rotation == 3) {
            board[row][col] = 2;
            board[row+1][col] = 2;
            board[row+2][col] = 2;
            board[row+3][col] = 2;
        } else {
            board[row][col] = 2;
            board[row][col+1] = 2;
            board[row][col+2] = 2;
            board[row][col+3] = 2;
        }
    } else if (type == 3) {
        if (rotation == 1 || rotation == 3) {
            board[row][col] = 3;
            board[row][col+1] = 3;
            board[row-1][col+1] = 3;
            board[row-1][col+2] = 3;
        } else {
            board[row][col] = 3;
            board[row+1][col] = 3;
            board[row+1][col+1] = 3;
            board[row+2][col+1] = 3;
        }
    } else if (type == 4) {
        if (rotation == 1) {
            board[row][col] = 4;
            board[row+1][col] = 4;
            board[row+2][col] = 4;
            board[row+2][col+1] = 4;
        } else if (rotation == 2) {
            board[row][col] = 4;
            board[row][col+1] = 4;
            board[row][col+2] = 4;
            board[row+1][col] = 4;
        } else if (rotation == 3) {
            board[row][col] = 4;
            board[row][col+1] = 4;
            board[row+1][col+2] = 4;
            board[row+2][col] = 4;
        } else {
            board[row][col] = 4;
            board[row][col+1] = 4;
            board[row][col+2] = 4;
            board[row-1][col+2] = 4;
        }
    } else if (type == 5) {
        if (rotation == 1 || rotation == 3) {
            board[row][col] = 5;
            board[row][col+1] = 5;
            board[row+1][col+1] = 5;
            board[row+1][col+2] = 5;
        } else {
            board[row][col] = 5;
            board[row][col+1] = 5;
            board[row-1][col+1] = 5;
            board[row+1][col] = 5;
        }
    } else if (type == 6) {
        if (rotation == 1) {
            board[row][col] = 6;
            board[row][col+1] = 6;
            board[row-1][col+1] = 6;
            board[row-2][col+1] = 6;
        } else if (rotation == 2) {
            board[row][col] = 6;
            board[row+1][col] = 6;
            board[row+1][col+1] = 6;
            board[row+1][col+2] = 6;
        } else if (rotation == 3) {
            board[row][col] = 6;
            board[row][col+1] = 6;
            board[row+1][col] = 6;
            board[row+2][col] = 6;
        } else {
            board[row][col] = 6;
            board[row][col+1] = 6;
            board[row][col+2] = 6;
            board[row+1][col+2] = 6;
        }
    } else {
        if (rotation == 1) {
            board[row][col] = 7;
            board[row][col+1] = 7;
            board[row][col+2] = 7;
            board[row+1][col+1] = 7;
        } else if (rotation == 2) {
            board[row][col] = 7;
            board[row-1][col+1] = 7;
            board[row][col+1] = 7;
            board[row+1][col+1] = 7;
        } else if (rotation == 3) {
            board[row][col] = 7;
            board[row-1][col+1] = 7;
            board[row][col+1] = 7;
            board[row][col+2] = 7;
        } else {
            board[row][col] = 7;
            board[row+1][col] = 7;
            board[row+2][col] = 7;
            board[row+1][col+1] = 7;
        }
    }
}

void process_initialize_packet(int other_client_sockfd, int client_sockfd, int player) {
    char buffer[BUFFER_SIZE];
    int ship_type, ship_rotation, ship_row, ship_col;
    
    while (1) {
        int bytes_read = read_message(client_sockfd, buffer, sizeof(buffer));
        if (bytes_read < 0) { return; }

        if (strncmp(buffer, "F", 1) == 0) { // Forfeit
            halt_response(client_sockfd, other_client_sockfd); // Send Halt response packets, close connections
            return;
        } else if (strncmp(buffer, "I", 1) == 0) {
            char * buffer_ptr = buffer + 1;

            int ships_placed = 0;
            while (ships_placed < 5) { // 's' for 'ship'
                if (sscanf(buffer_ptr, "%d %d %d %d", &ship_type, &ship_rotation, &ship_row, &ship_col) == 4) {

                    if (ship_type < 1 || ship_type > 7) {
                        error_response(client_sockfd, 300); // Invalid Initialize Packet (shape out of range)
                        break;
                    } else if (ship_rotation < 1 || ship_rotation > 4) {
                        error_response(client_sockfd, 301); // Invalid Initialize Packet (rotation out of range)
                        break;
                    } else if (ship_row < 0 || ship_row >= board_height || ship_col < 0 ||ship_col >= board_width){
                        error_response(client_sockfd, 302); // Invalid Initialize Packet (ship does not fit board)
                        break;
                    } else if (!check_within_border(ship_type, ship_rotation, ship_row, ship_col)) {
                        error_response(client_sockfd, 302); // Invalid Initialize Packet (ship does not fit board)
                        break;
                    } else if (!check_no_overlap(ship_type, ship_rotation, ship_row, ship_col, (player == 1) ? p1_board : p2_board)) {
                        error_response(client_sockfd, 303); // Invalid Initialize Packet (ships overlap)
                        break;
                    } else {
                        place_ship(ship_type, ship_rotation, ship_row, ship_col, (player == 1) ? p1_board : p2_board);
                        ships_placed++;

                        while (*buffer_ptr != ' ' && *buffer_ptr != '\0') {
                            buffer_ptr++;
                        }
                        if (*buffer_ptr == ' ') {
                            buffer_ptr++;
                        }
                    }
                } else {
                    error_response(client_sockfd, 201); // Invalid Initialize Packet (invalid parameters)
                    break;
                }
            }
            if (ships_placed == 5) {
                return;
            }
        } else {
            error_response(client_sockfd, 101); // Invalid Packet Type (Expected initialize packet) 
        }
    }
}

bool process_shoot_packet(int other_client_sockfd, int client_sockfd, int player, int row, int col) {
    int ** opponent_board = (player == 1) ? (int **)p2_board : (int**)p1_board;
    int * opponent_ships_remaining = (player == 1) ? &p2_ships_remaining : &p1_ships_remaining;
    
    int state = opponent_board[row][col];
    int ships_remain = false; // Check if the last ship is gone
    int ship_remains = false; // Check if there are still parts of the current ship left
    
    if (state == MISS || state == HIT) {
        error_response(client_sockfd, 401);
        return false;
    }
    
    char shot_response[BUFFER_SIZE]; 

    if (state == 0) {
        opponent_board[row][col] = MISS; // Mark the shot as a miss
        snprintf(shot_response, sizeof(shot_response), "R %d M", *opponent_ships_remaining); // Format the shot_response
        write_message(client_sockfd, shot_response);
        return false; // Return false if ships are left (this shot did not hit anything)
    } else {
        opponent_board[row][col] = HIT; // Mark the shot as a hit

        for (int r = 0; r < board_height; r++) { // Check if a part of the ship remains
            for (int c = 0; c < board_width; c++) {
                if (opponent_board[r][c] == state) {
                    ship_remains = true;
                    break;
                }
            }
        }
        if (!ship_remains) { // If no parts of the ship remain, decrement the remaining ships count
            
            *opponent_ships_remaining = *opponent_ships_remaining - 1;
        }

        for (int r = 0; r < board_height; r++) { // Check if ships still remain
            for (int c = 0; c < board_width; c++) {
                if (opponent_board[r][c] != 0 && opponent_board[r][c] != -1 && opponent_board[r][c] != -2) {
                    ships_remain = true;
                    break;
                }
            }
        }
        if (!ships_remain) {
            snprintf(shot_response, sizeof(shot_response), "R %d H", *opponent_ships_remaining); // Format the shot_response
            write_message(client_sockfd, shot_response);
            read_message(other_client_sockfd, NULL, 0); // Read from the other socket without storing it
            write_message(other_client_sockfd, "H 0\n"); // Send a (loss) Halt Packet to P1
            read_message(client_sockfd, NULL, 0); // Read from the other socket without storing it
            write_message(client_sockfd, "H 1\n"); // Send a (win) Halt Packet to P2

            free_board(p1_board, board_height);
            free_board(p2_board, board_height);
            // Close the connection to the ports after sending the Halt packets
            close(client_sockfd);  // Close Player A's connection
            close(other_client_sockfd);  // Close Player B's connection
            return true; // Return true if no ships are left
        }
        snprintf(shot_response, sizeof(shot_response), "R %d H", *opponent_ships_remaining); // Format the shot_response
        write_message(client_sockfd, shot_response);
        return false; // Return false if there are still ships left
    }
}

void process_query_packet(int client_sockfd, int player) {
    int ** opponent_board = (player == 1) ? (int **)p2_board: (int **)p1_board;
    int opponent_ships_remaining = (player == 1) ? p2_ships_remaining : p1_ships_remaining;
    int state = 0;
    
    char query_response[BUFFER_SIZE] = {0};
    snprintf(query_response, sizeof(query_response), "G %d", opponent_ships_remaining);

    for (int r = 0; r < board_height; r++) {
        for (int c = 0; c < board_width; c++) {
            if (opponent_board[r][c] == MISS) {
                snprintf(query_response + strlen(query_response), sizeof(query_response) - strlen(query_response), " M %d %d", c, r);
            } else if (opponent_board[r][c] == HIT) {
                snprintf(query_response + strlen(query_response), sizeof(query_response) - strlen(query_response), " H %d %d", c, r);
            }
        }
    }
    write_message(client_sockfd, query_response);
}

// void play_game(int other_client_sockfd, int client_sockfd, int player) {
//     char buffer[BUFFER_SIZE];

//     while (1) {
//         int bytes_read = read_message(client_sockfd, buffer, sizeof(buffer));
//         if (bytes_read < 0) { return; }

//         if (strncmp(buffer, "S", 1) == 0) {
//             int shoot_row = 0;
//             int shoot_col = 0;
//             if (sscanf(buffer, "S %d %d", shoot_row, shoot_col) == 2) {
//                 if (shoot_row < 0 || shoot_row >= board_height || shoot_col < 0 || shoot_col >= board_width) {
//                     error_response(client_sockfd, 400); // Invalid Shoot pakcet (cell not in game board)
//                 }
//                 process_shoot_packet(other_client_sockfd, client_sockfd, player, shoot_row, shoot_col);
//             } else {
//                 error_response(client_sockfd, 102);
//             }
//         } else if (strncmp(buffer, "Q", 1) == 0) {
//             if (sscanf(buffer, "Q") == 0) {
//                 process_query_packet(client_sockfd, player);
//             } else {
//                 error_response(client_sockfd, 102);
//             }
//         } else if (strncmp(buffer, "F", 1) == 0) {
//             if (sscanf(buffer, "F") == 0) {
//                 halt_response(client_sockfd, other_client_sockfd);
//                 return;
//             } else {
//                 error_response(client_sockfd, 102);
//             }
//         } else {
//             error_response(client_sockfd, 102);
//         }
//     }
// }

int main() {
    int player1 = create_socket(PORT1);
    int player2 = create_socket(PORT2);

    struct sockaddr_in client_address1;
    struct sockaddr_in client_address2;
    socklen_t client_addr_len1 = sizeof(client_address1);
    socklen_t client_addr_len2 = sizeof(client_address2);

    printf("Waiting for player 1 to connect...\n");
    int connect_player1 = accept(player1, (struct sockaddr *)&client_address1, &client_addr_len1);
    if (connect_player1 < 0) {
        perror("Player 1 failed to accept");
    } else {
        printf("Player 1 connected\n");
    }

    printf("Waiting for player 2 to connect...\n");
    int connect_player2 = accept(player2, (struct sockaddr *)&client_address2, &client_addr_len2);
    if (connect_player2 < 0) {
        perror("Player 2 failed to accept");
    } else {
        printf("Player 2 connected\n");
    }

    if (process_begin_packet(connect_player2, connect_player1, 1) == 0) {
        return 0;
    }
    if (process_begin_packet(connect_player1, connect_player2, 2) == 0) {
        return 0;
    }

    process_initialize_packet(connect_player2, connect_player1, 1);
    process_initialize_packet(connect_player1, connect_player2, 2);

    char buffer[BUFFER_SIZE];
    while (1) {
        int bytes_read = read_message(connect_player1, buffer, sizeof(buffer));
        if (bytes_read < 0) { return -1; }

        if (strncmp(buffer, "S", 1) == 0) {
            int shoot_row = 0;
            int shoot_col = 0;
            if (sscanf(buffer, "S %d %d", shoot_row, shoot_col) == 2) {
                if (shoot_row < 0 || shoot_row >= board_height || shoot_col < 0 || shoot_col >= board_width) {
                    error_response(connect_player1, 400); // Invalid Shoot pakcet (cell not in game board)
                } else {
                    if (process_shoot_packet(connect_player2, connect_player1, 1, shoot_row, shoot_col)) {
                        return 0;
                    }
                }
            } else {
                error_response(connect_player1, 102);
            }
        } else if (strncmp(buffer, "Q", 1) == 0) {
            if (sscanf(buffer, "Q") == 0) {
                process_query_packet(connect_player1, 1);
            } else {
                error_response(connect_player1, 102);
            }
        } else if (strncmp(buffer, "F", 1) == 0) {
            if (sscanf(buffer, "F") == 0) {
                halt_response(connect_player1, connect_player2);
                return 0;
            } else {
                error_response(connect_player1, 102);
            }
        } else {
            error_response(connect_player1, 102);
        }

        bytes_read = read_message(connect_player2, buffer, sizeof(buffer));
        if (bytes_read < 0) { return -1; }

        if (strncmp(buffer, "S", 1) == 0) {
            int shoot_row = 0;
            int shoot_col = 0;
            if (sscanf(buffer, "S %d %d", shoot_row, shoot_col) == 2) {
                if (shoot_row < 0 || shoot_row >= board_height || shoot_col < 0 || shoot_col >= board_width) {
                    error_response(connect_player2, 400); // Invalid Shoot pakcet (cell not in game board)
                } else {
                    if (process_shoot_packet(connect_player1, connect_player2, 2, shoot_row, shoot_col)) {
                        return 0;
                    }
                }
            } else {
                error_response(connect_player2, 102);
            }
        } else if (strncmp(buffer, "Q", 1) == 0) {
            if (sscanf(buffer, "Q") == 0) {
                process_query_packet(connect_player2, 2);
            } else {
                error_response(connect_player2, 102);
            }
        } else if (strncmp(buffer, "F", 1) == 0) {
            if (sscanf(buffer, "F") == 0) {
                halt_response(connect_player2, connect_player1);
                return 0;
            } else {
                error_response(connect_player2, 102);
            }
        } else {
            error_response(connect_player2, 102);
        }
    }

    
    return 0;
}

