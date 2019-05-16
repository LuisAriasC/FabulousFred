/*
Luis Carlos Arias Camacho
A01364808
Final Project
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <sys/poll.h>
#include <pthread.h>

// Custom libraries
#include "status_codes.h"
#include "sockets.h"
#include "fatal_error.h"



//Structure definitions
#define MAX_PLAYERS 3
#define BUFFER_SIZE 1024
#define MAX_QUEUE 15

int exit_flag = 0;


// Data for a player
typedef struct player_struct{
    int file_descriptor;
    int t_status;
    int g_status;
} player_t;


// Player "constructor"
player_t * newPlayer(int player_fd)
{
    player_t * player = malloc (sizeof(player_t));
    player->file_descriptor = player_fd;
    player->t_status = inactive_turn;
    player->g_status = inactive_game;
    return player;
}


// Data for a game
typedef struct game_struct{
    int turn;
    int num_players;
    player_t ** players;
    int sequence_size;
    char * sequence;
    int current_seq;
    int winner;
} game_t;


// Allocate a new game
bool allocateGame(game_t * game, int players){

    game->turn = 0;
    game->num_players = players;
    game->sequence_size = 1;
    game->current_seq = 0;
    game->winner = -1;

    // Allocate player array
    game->players = (player_t **)malloc(players * sizeof(player_t *));
    if (game->players == NULL)
    {
        printf("ERROR: Could not allocate memory for players array!\n");
        return false;
    }

    // Assign pointers to every row
    for (int i = 0; i < players; i++)
        game->players[i] = game->players[0] + 1 * i;

    // CALLOC NEW SEQUENCE
    game->sequence = (char *)calloc(BUFFER_SIZE,sizeof(char));

    return true;
}

// Add player to board
void addPlayer(game_t * game, player_t * player, int index)
{
    game->players[index] = player;
}

// FREE THE GAME
void freeGame(game_t * game){

    // Free players
    for (int i = 0; i < game->num_players; i++)
        free(game->players[i]);

    free(game->sequence);

    game->turn = 0;
    game->num_players = 0;
    game->sequence_size = 0;
    bzero(game->sequence, BUFFER_SIZE);

    // Free game
    free(game->players[0]);
	free(game->players);
}


/* NOT USED */
// Structure for the mutexes to keep the data consistent
typedef struct locks_struct {
    // Mutex to get thread_id
    pthread_mutex_t thread_id_mutex;
    // Mutex array for the players
    pthread_mutex_t * player_mutex;
} locks_t;

/*  NOT USED  */
// Data that will be sent to each structure
typedef struct data_struct {
    // The file descriptor for the socket
    int file_descriptor;
    // The turn of the player
    int turn;
    // A pointer to a game data structure
    game_t * game;
} thread_data_t;


// Global variables for signal handlers
int interrupt_exit = 0;


///// FUNCTION DECLARATIONS
void usage(char * program);
void setupHandlers();
void onInterrupt(int signal);
void setupGame(game_t * game_data);
void initGame(game_t * game_data);
void waitForConnections(int server_fd);
void startgame(game_t * game);
void * attentionThread(void * arg);
bool wonGame(game_t * game);
int nextTurn(game_t * game);
int increaseSequence(thread_data_t * connection_data, int tid);
int getThreadID(thread_data_t * connection_data);
int getActiveTurn(thread_data_t * connection_data, int tid);
int getActiveGame(thread_data_t * connection_data, int tid);
void attendPoll(thread_data_t * connection_data, int client_fd, int players, int tid );


///// MAIN FUNCTION
int main(int argc, char * argv[])
{
    int server_fd;

    printf("\nFABULOUS FRED\n");

    // Check the correct arguments
    if (argc != 2)
    {
        usage(argv[0]);
    }

    // Configure the handler to catch SIGINT
    setupHandlers();

	// Show the IPs assigned to this computer
	printLocalIPs();

    // Start the server
    server_fd = initServer(argv[1], MAX_QUEUE);

	// Listen for connections from the clients
    waitForConnections(server_fd);

    // Close the socket
    close(server_fd);

    // Finish the main thread
    pthread_exit(NULL);

    return 0;
}

///// FUNCTION DEFINITIONS

/*
    Explanation to the user of the parameters required to run the program
*/
void usage(char * program)
{
    printf("Usage:\n");
    printf("\t%s {port_number}\n", program);
    exit(EXIT_FAILURE);
}

/*
CHANGE BEHAVIOUR OF CTRL+C
*/
void onInterrupt(int signal)
{
    exit_flag = 1;
}


/*
SET UP NEW HANDLERS FOR THE PROGRAM
*/
void setupHandlers()
{
   struct sigaction new_action;
   struct sigaction old_action;
   
   // Prepare the structure to handle a signal
   new_action.sa_handler = onInterrupt;
   new_action.sa_flags = 0;
   sigfillset(&new_action.sa_mask);
   
   // Catch Ctrl-C
   sigaction(SIGINT, &new_action, &old_action);
}

// APPEND CHAR TO STRING
void append(char* s, char c) {
        int len = strlen(s);
        s[len] = c;
        s[len+1] = '\0';
}

/*
GET INITIL RANDOM CHAR FOR THE GAME
*/
static const char ucase[] = "abcdefghijklmnop";
char getRandomChar(){
    srand(time(NULL));
    const size_t ucase_count = sizeof(ucase) - 1;
    char random_char;
    int random_index = (double)rand() / RAND_MAX * ucase_count;     
    random_char = ucase[random_index];
    return random_char;
}

/*
    Main loop to wait for incomming connections
*/
void waitForConnections(int server_fd)
{
    struct sockaddr_in client_address;
    socklen_t client_address_size;
    char client_presentation[INET_ADDRSTRLEN];
    int connection_fd;

    // Get the size of the structure to store client information
    int i = 0;
    client_address_size = sizeof client_address;
    int num = MAX_PLAYERS;
    // Loop waiting for connections

    while (!exit_flag)
    {
        // Start new game
        game_t game;
        allocateGame(&game,num);
        // WAIT FOR MAX_PLAYER NUMBER
        while (!exit_flag)
        {
            connection_fd = accept(server_fd, (struct sockaddr *) &client_address, &client_address_size);
            player_t * player = newPlayer(connection_fd);
            if ( player->file_descriptor == -1 )
            {
                perror("ERROR: accept");
                exit(EXIT_FAILURE);
            }

            // Get the ip address from client connection
            inet_ntop(client_address.sin_family, &client_address.sin_addr, client_presentation, sizeof client_presentation);
            printf(" %d - New client from %s in port %d\n", i , client_presentation, client_address.sin_port);
            if (i == 0)
            {
                player->t_status = 1;
                player->g_status = 1;
            }
            else
            {
                player->t_status = 0;
                player->g_status = 1;
            }
            // ADD PLAYER TO GAME
            addPlayer(&game, player, i);
            i++;

            if (i == num)
                break;
        }

        // OPEN FILE DESCRIPTORS TO PASS DATA FROM PARENT TO CHILD
        i = 0;
        int fd_pc[2];
        pipe(fd_pc);

        //FORK
        pid_t child = fork();
        if(child < 0) {
            perror("ERROR. fork()");
            exit(EXIT_FAILURE);
        }

        // Parent process
        else if (child > 0) 
        {
            // Write only! Close read descriptor
            close(fd_pc[0]);
            // Send struct to child
            write(fd_pc[1], &game, sizeof(game));
            // Close the write descriptor 
            close(fd_pc[1]);
            // Free game in parent process    
            freeGame(&game);
        }
        else { //child process
            // NEW GAME IN THE FORK
            game_t game_c;
            // Close write descriptor 
            close(fd_pc[1]);
            // Read parameters
            read(fd_pc[0], &game_c, sizeof(game_c));
            // Close read descriptor
            close(fd_pc[0]);
            // Start game
            startgame(&game_c);              
        }
    }
}

// HANDLE THE GAME WITH A POLL
void startgame(game_t * game)
{
    pthread_t new_tid[game->num_players];

    // Get the first letter of the game
    append(game->sequence, getRandomChar());

    thread_data_t ** connection_data_ar = (thread_data_t **)malloc(game->num_players * sizeof(thread_data_t));

    // Assign pointers to every row
    for (int i = 0; i < game->num_players; i++)
        connection_data_ar[i] = connection_data_ar[0] + 1 * i;

    // Assign game to every struct 
    for (int i = 0; i < game->num_players; i++)
    {
        connection_data_ar[i]->game = game;
        connection_data_ar[i]->file_descriptor = game->players[i]->file_descriptor;
        connection_data_ar[i]->turn = i;
    }

    //  Create threads to handle clients
    for (int i = 0; i < game->num_players; i++)
    {
        pthread_create(&new_tid[i], 0, attentionThread, (void *)connection_data_ar[i]);
    }

    // Wait for the threads to finish
    for (int i=0; i < game->num_players; i++)
    {
        pthread_join(new_tid[i], NULL);
    }
    
    freeGame(game);
    exit(0);
}


void * attentionThread(void * arg)
{
    thread_data_t * connection_data = (thread_data_t *) arg;

    long int current = 0;
    char buffer[BUFFER_SIZE];
    int next_turn;
    int status;

    int turn = connection_data->turn;
    int current_turn;
    int connection_fd = connection_data->file_descriptor;

    //printf("In attention thread %d %d\n", turn, connection_fd);

    struct pollfd ff_poll[1];
    ff_poll[0].fd = connection_data->game->players[turn]->file_descriptor;
    ff_poll[0].events = POLLIN;

    // Send game info to client in order to start the game
    bzero(buffer, BUFFER_SIZE);
    sprintf(buffer, "%d %d %d %c", connection_data->game->players[turn]->g_status, connection_data->game->players[turn]->t_status, turn , connection_data->game->sequence[0]);
    printf(" Sending as first sequence: %s\n", buffer);
    sendString(connection_fd, buffer, strlen(buffer)+1);

    int win_flag = 0;
    int timeout = 500;
    char recieved_char;

    // START GAME HANDLER
    while( !exit_flag )
    {
        status = poll(ff_poll, 1, timeout);
        // Handle errors
        if (status < 0)
        {
            // Handle if server exits
            if (errno == EINTR && exit_flag)
            {
                printf("LEAVING!!!\n");
                // LEAVING PROGRAM
                bzero(&buffer, BUFFER_SIZE);
                sprintf(buffer, "%d %d %d %c %d",0,0,0,'*',0);
                //Send the exit sstatis to clients
                sendString(connection_fd, buffer, strlen(buffer) + 1);
                break;
            }
            else
                fatalError("ERROR: poll");
        }
        // Recieving signal in poll
        else if (status > 0)
        {
            // Handle answer from incoming poll
            if (ff_poll[0].revents & POLLIN) 
            {
                bzero(&buffer, BUFFER_SIZE);
                // RECIEVES DATA
                if( !recvString( connection_fd, buffer, BUFFER_SIZE))
                {
                    printf("Player loosed the game or closed the connection\n");
                    connection_data->game->players[turn]->g_status = 0;
                    connection_data->game->players[turn]->t_status = 0;

                    next_turn = nextTurn(connection_data->game);
                    if (wonGame(connection_data->game))
                    {
                         win_flag = 1;
                         connection_data->game->players[connection_data->game->winner]->g_status = 2;
                    }
                    else
                    {   // ASSIGN NEW TURN
                        connection_data->game->turn = next_turn;
                        connection_data->game->players[connection_data->game->turn]->t_status = 1;
                    }
                    if (!win_flag)
                    {
                        // SEND NEW TURN TO ALL PLAYERS
                        for (int k = 0; k < connection_data->game->num_players; k++)
                        {
                            if (k != connection_data->game->turn)
                            {
                                if (connection_data->game->players[k]->g_status == 1)
                                {
                                    bzero(buffer, BUFFER_SIZE);
                                    sprintf(buffer, "%d %d %d %c %d", connection_data->game->players[k]->g_status, connection_data->game->players[k]->t_status, connection_data->game->turn, '*', 1);
                                    sendString(connection_data->game->players[k]->file_descriptor, buffer, strlen(buffer)+1);
                                }
                            }
                        }
                        // CHANGE TURN
                        bzero(buffer, BUFFER_SIZE);
                        sprintf(buffer, "%d %d %d %c %d", connection_data->game->players[connection_data->game->turn]->g_status, connection_data->game->players[connection_data->game->turn]->t_status, connection_data->game->turn, '*', 1);
                        sendString(connection_data->game->players[connection_data->game->turn]->file_descriptor, buffer, strlen(buffer)+1);
                    }
                    // SEND CONFIRMATION TO PLAYER THAT WON THE connection_data->game
                    else
                    {
                        bzero(buffer, BUFFER_SIZE);
                        sprintf(buffer, "%d %d %d %c %d", connection_data->game->players[connection_data->game->winner]->g_status, connection_data->game->players[connection_data->game->winner]->t_status, connection_data->game->turn, 'X', 1);
                        sendString(connection_data->game->players[connection_data->game->winner]->file_descriptor, buffer, strlen(buffer)+1);
                    }
                    
                    break;
                    //break;
                }
                sscanf(buffer, "%d %d %d %c", &connection_data->game->players[turn]->g_status , &connection_data->game->players[turn]->t_status, &current_turn ,&recieved_char);
                connection_data->game->turn = turn;
                printf("\nbuffer %s\n", buffer);

                if (connection_data->game->players[turn]->g_status)
                {
                    printf(".\n");
                    printf("current %ld    len %ld\n", current, strlen(connection_data->game->sequence));
                    // HANDLE PLAYER INPUT
                    if (current < strlen(connection_data->game->sequence))
                    {
                        printf("..\n");
                        // Compare imput char with the current hit on the sequence
                        if (!(recieved_char == connection_data->game->sequence[current]))
                        {
                            printf("...\n");
                            // Input not equal to sequence position
                            // End turn of the current player
                            connection_data->game->players[turn]->g_status = 0;
                            // Check if the next player won
                            next_turn = nextTurn(connection_data->game);
                            if (wonGame(connection_data->game))
                            {
                                 win_flag = 1;
                                 connection_data->game->players[connection_data->game->winner]->g_status = 2;
                            }
                            else
                            {   // ASSIGN NEW TURN
                                connection_data->game->turn = next_turn;
                                connection_data->game->players[connection_data->game->turn]->t_status = 1;
                            }
                            if (!win_flag)
                            {
                                // SEND NEW CHAR IN SEQUENCE
                                for (int k = 0; k < connection_data->game->num_players; k++)
                                {
                                    if (k != connection_data->game->turn)
                                    {
                                        if (connection_data->game->players[k]->g_status == 1)
                                        {
                                            bzero(buffer, BUFFER_SIZE);
                                            sprintf(buffer, "%d %d %d %c %d", connection_data->game->players[k]->g_status, connection_data->game->players[k]->t_status, connection_data->game->turn, '*', 1);
                                            sendString(connection_data->game->players[k]->file_descriptor, buffer, strlen(buffer)+1);
                                        }
                                    }
                                }
                                // CHANGE TURN
                                bzero(buffer, BUFFER_SIZE);
                                sprintf(buffer, "%d %d %d %c %d", connection_data->game->players[connection_data->game->turn]->g_status, connection_data->game->players[connection_data->game->turn]->t_status, connection_data->game->turn, '*', 1);
                                sendString(connection_data->game->players[connection_data->game->turn]->file_descriptor, buffer, strlen(buffer)+1);
                            }
                            // SEND CONFIRMATION TO PLAYER THAT WON THE connection_data->game
                            else
                            {
                                printf("....\n");
                                bzero(buffer, BUFFER_SIZE);
                                sprintf(buffer, "%d %d %d %c %d", connection_data->game->players[connection_data->game->winner]->g_status, connection_data->game->players[connection_data->game->winner]->t_status, connection_data->game->turn, 'X', 1);
                                sendString(connection_data->game->players[connection_data->game->winner]->file_descriptor, buffer, strlen(buffer)+1);
                            }       
                        }
                        else
                        {
                            printf("SON IGUALES\n");
                            // NEW INPUT IN CURRENT TURN
                            for (int o = 0; o < connection_data->game->num_players; o++)
                            {
                                if (o != turn)
                                {   
                                    if (connection_data->game->players[o]->g_status == 1)
                                    {
                                        // SEND SEQUENCE
                                        bzero(buffer, BUFFER_SIZE);
                                        sprintf(buffer, "%d %d %d %c %d", connection_data->game->players[o]->g_status, connection_data->game->players[o]->t_status, connection_data->game->turn, recieved_char, 0 );
                                        printf("%d SENDING: %s\n", o, buffer);
                                        sendString(connection_data->game->players[o]->file_descriptor, buffer, strlen(buffer)+1);
                                    }
                                }
                            }
                            current ++;
                        }
                    }
                    else
                    {
                        printf("New char\n");
                        // GENERATE NEW INPUT
                        append(connection_data->game->sequence,recieved_char);
                        // Get the next turn
                        next_turn = nextTurn(connection_data->game);
                        // Send new turn status to all players that will still wait for inputs
                        printf("Turn %d \n", connection_data->game->turn);
                        printf("Next turn %d \n", next_turn);
                        for (int p = 0; p < connection_data->game->num_players; p++)
                        {
                            if (p != turn && p != next_turn)
                            {
                                if (connection_data->game->players[p]->g_status != 0)
                                {
                                    bzero(buffer, BUFFER_SIZE);
                                    sprintf(buffer, "%d %d %d %c %d", connection_data->game->players[p]->g_status, connection_data->game->players[p]->t_status, next_turn, recieved_char, 1);
                                    printf("%d Sending %s\n", p,buffer);
                                    sendString(connection_data->game->players[p]->file_descriptor, buffer, strlen(buffer)+1);
                                }
                            }
                        }
                        // SEND NEW CHAR TO THE NEW CURRENT PLAYER
                        connection_data->game->players[turn]->t_status = 0;
                        connection_data->game->players[next_turn]->t_status = 1;
                        bzero(buffer, BUFFER_SIZE);
                        sprintf(buffer, "%d %d %d %c %d", connection_data->game->players[next_turn]->g_status, connection_data->game->players[next_turn]->t_status, next_turn, recieved_char, 1);
                        printf("%d Sending to new current %s\n",next_turn, buffer);
                        sendString(connection_data->game->players[next_turn]->file_descriptor, buffer, strlen(buffer)+1);
                        current = 0;
                        connection_data->game->turn = next_turn;
                        // next_turn = 0;
                    }
                    bzero(buffer, BUFFER_SIZE);
                    if (connection_data->game->players[turn]->t_status)
                    {
                        sprintf(buffer, "%d %d %d %c %d", connection_data->game->players[turn]->g_status, connection_data->game->players[turn]->t_status, connection_data->game->turn, '*', 0);
                    }
                    else
                    {
                        sprintf(buffer, "%d %d %d %c %d", connection_data->game->players[turn]->g_status, connection_data->game->players[turn]->t_status, connection_data->game->turn, '*', 1);
                    }
                    printf("Sending to current client %s\n", buffer);
                    sendString(connection_data->game->players[turn]->file_descriptor, buffer, strlen(buffer)+1);
                }
                else
                {
                    printf("  ** ** ** CLIENT DISCONECTED ** ** **\n");
                    connection_data->game->players[turn]->g_status = 0;
                    connection_data->game->players[turn]->t_status = 0;
                    break;
                }
            }      
        }
    }
    pthread_exit(NULL);
}



// Check if the next player won the game
bool wonGame(game_t * game){

    int count = 0;
    for (int i = 0; i < game->num_players; i++)
        if (game->players[i]->g_status == 1)
            count++;
    
    int index;
    if (count == 1)
    {
        for (int i = 0; i < game->num_players; i++)
        {
            if (game->players[i]->g_status == 1)
            {
                index = i;
                break;
            }    
        }
        game->winner = index;
        return true;
    }
    return false;
}


// Set the next turn in the game
int nextTurn(game_t * game)
{
    //printf("Current turn %d\n", game->turn);
    int current = game->turn;
    while (1)
    {
        if (current + 1 == game->num_players)
            current = 0;
        else
            current++;
        if (game->players[current]->g_status == 1)
            break;
    }
    return current;
}
