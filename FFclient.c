/*
Luis Carlos Arias Camacho
A01364808
Final Project
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "sockets.h"
#include "fatal_error.h"
#include <sys/poll.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <ncurses.h>

#define BUFFER_SIZE 1024

//EXIT FLAG OF THE PROGRAM TO CATCH CTRL+C
int exit_flag = 0;

///// FUNCTION DECLARATIONS
void usage(char * program);
void startGame(int connection_fd);
void setupHandlers();
void onInterrupt(int signal);

///// MAIN FUNCTION
int main(int argc, char * argv[])
{
    int connection_fd;

    //printf("\nFABULOUS FRED CLIENT\n");

    // Check the correct arguments
    if (argc != 3)
    {
        usage(argv[0]);
    }

    // Configure the handler to catch SIGINT
    //setupHandlers();

    // Start the server
    connection_fd = connectSocket(argv[1], argv[2]);
	// Start the game
    startGame(connection_fd);
    // Close the socket
    close(connection_fd);

    return 0;
}


/*
    Explanation to the user of the parameters required to run the program
*/
void usage(char * program)
{
    printf("Usage:\n");
    printf("\t%s {server_address} {port_number}\n", program);
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

void printBoardHL(WINDOW * w, int init_row, int box_margin, char recieved_char)
{
    int ascii = 97;
    for (int row = 0; row < 4; row++)
    {
        for (int col = 1; col < 5; col++)
        {
            if (recieved_char == ascii)
            {
                wattron( w, A_STANDOUT );
                mvwprintw( w, init_row + (2 * row) , box_margin * col, "%c", ascii );
                wattroff( w, A_STANDOUT );
                ascii++;
            }
            else
            {
                mvwprintw( w, init_row + (2 * row) , box_margin * col, "%c", ascii );
                ascii++;
            }
        }
    }


}

void printBoard(WINDOW * w, int init_row, int box_margin)
{
    int ascii = 97;
    for (int row = 0; row < 4; row++)
    {
        for (int col = 1; col < 5; col++)
        {
            mvwprintw( w, init_row + (2 * row) , box_margin * col, "%c", ascii );
            ascii++;
        }
    }

}

void printScreen(WINDOW * w, int start_col, char turn[] )
{
    mvwprintw( w, 2, start_col, "%s", turn ); 
    //printf("\t\tIn screen printer\n");
}

/*
    Main menu with the options available to the user
*/
void startGame(int connection_fd)
{
    char buffer[BUFFER_SIZE];
    char myTurnBuffer[BUFFER_SIZE];
    char currentTurnBuffer[BUFFER_SIZE];

    int game_status = 0;
    int turn_status = 0;
    char send_char = '*';
    int my_turn = 0;
    int player_turn = 0;
    char recieved_char = '*';
    int first_hit = 0;

    // POLL STRUCTURE TO CATCH DATA ENTRIES FROM SERVER
	int timeout = 500;		// Time in milliseconds (0.5 seconds)
    int status;
    struct pollfd ff_poll;
    ff_poll.fd = connection_fd;
    ff_poll.events = POLLIN;

    struct winsize win;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &win);

    int margin = 2;
    //printf ("lines %d\n", win.ws_row);
    //printf ("columns %d\n", win.ws_col);
    int cols = win.ws_col - (margin * 2);
    int rows = win.ws_row - (margin * 2);

    int box_margin = cols / 5;
    int init_row = 4;
    int turn_row = rows - 6;
    int turn_col = (cols / 2) - 5;
    int start_name = ((cols/2) + (margin / 2)) - (strlen(myTurnBuffer)/2) - 1;
    int new_turn = 0;

    WINDOW *w;
    initscr(); // initialize Ncurses
    w = newwin( rows, cols, margin, margin ); // create a new window
    box( w, 0, 0 ); // sets default borders for the window
    wrefresh( w ); // update the terminal screen
 
    noecho(); // disable echoing of characters on the screen
    keypad( w, TRUE ); // enable keyboard input for the window.
    curs_set( 0 ); // hide the default screen cursor.

    printBoard(w, init_row, box_margin);
    mvwprintw( w, turn_row , turn_col - 2, "%s", "WAITING FOR MORE PLAYERS..." );
    wrefresh( w ); // update the terminal screen


    // get the input
    while (!exit_flag)
    {
        status = poll(&ff_poll, 1, timeout);
        // Handle errors
        if (status < 0)
        {
            // Handle if  client exits
            if (errno == EINTR && exit_flag)
            {
                werase(w);
                mvwprintw(w, rows / 2, (cols / 2) - (strlen("GOODBYE!!!") / 2 + 1), "GOODBYE!!!");
                wrefresh( w ); // update the terminal screen
                sleep(3);
                game_status = 0;
                bzero(buffer, BUFFER_SIZE);
                sprintf(buffer, "%d %d %d %c", game_status, turn_status, my_turn, send_char);
                sendString(connection_fd, buffer, strlen(buffer)+1);
                break;
            }
            else
            {
                fatalError("ERROR: poll");
                break;
            }   
        }
        // Recieving signal in poll
        else if (status > 0)
        {

            if (ff_poll.revents & POLLIN) 
            {
                werase(w);
                bzero(&buffer, BUFFER_SIZE);
                if( !recvString(connection_fd, buffer, BUFFER_SIZE) )
                {
                    printf("Server closed the connection\n");   
                    break;
                }
                sscanf(buffer, "%d %d %d %c %d",&game_status, &turn_status, &player_turn, &recieved_char, &new_turn);


                if (game_status == 1)
                {
                    if (!first_hit)
                    {
                        bzero(myTurnBuffer, BUFFER_SIZE);
                        sprintf(myTurnBuffer, "Player %d", player_turn);
                        bzero(currentTurnBuffer, BUFFER_SIZE);
                        sprintf(currentTurnBuffer, "Current turn: Player %d", 0);
                        first_hit = 1;
                    }
                    else
                    {
                        bzero(currentTurnBuffer, BUFFER_SIZE);
                        sprintf(currentTurnBuffer, "Current turn: Player %d", player_turn);
                        wrefresh( w ); // update the terminal screen
                    }

                    mvwprintw( w, 2, start_name, "%s", myTurnBuffer );
                    //printBoard(w, init_row, box_margin);
                    //wrefresh( w ); // update the terminal screen
                    //sleep(1);
                    if (turn_status)
                    {   
                        bzero(currentTurnBuffer, BUFFER_SIZE);
                        sprintf(currentTurnBuffer, "Current turn: Player %d", player_turn);
                        mvwprintw( w, turn_row , 6,"%s", currentTurnBuffer);
                        printBoardHL(w, init_row, box_margin, recieved_char);
                        wrefresh( w ); // update the terminal screen
                        sleep(2);
                        printBoard(w, init_row, box_margin);
                        mvwprintw( w, turn_row , turn_col  - 2, "%s", "YOUR TURN! Repeat sequence & add one" );
                        while (1)
                        {
                            mvwprintw( w, turn_row + 3, turn_col -2, "%s", "CLICK KEYBOARD CHAR :   ");
                            wrefresh( w ); // update the terminal screen
                            send_char = wgetch(w);
                            if ( send_char >= 97 && send_char < 97 + 16)
                            {
                                mvwprintw( w, turn_row + 3, turn_col + 20, "%c", send_char);
                                break;
                            }
                            else
                            {
                                mvwprintw( w, turn_row + 3, turn_col -2, "%s", "CLICK FROM a TO p   :   ");
                            }
                        }
                        wrefresh( w ); // update the terminal screen
                        sleep(0.5);

                        // Send move
                        bzero(buffer, BUFFER_SIZE);
                        sprintf(buffer, "%d %d %d %c", game_status, turn_status, my_turn, send_char);
                        sendString(connection_fd, buffer, strlen(buffer)+1);
                        
                    }
                    // READ ONLY
                    else
                    {
                        printBoardHL(w, init_row, box_margin, recieved_char);
                        mvwprintw( w, turn_row , turn_col - 2, "%s", "NOT YOUR TURN!                        " );
                        mvwprintw( w, turn_row + 3, turn_col -2, "%s", "                        ");
                        mvwprintw( w, turn_row , 6,"%s", currentTurnBuffer);
                        if (new_turn == 1)
                        {
                            mvwprintw( w, turn_row + 3 , 6, "%s", "Turn changed!");
                        }
                        wrefresh( w ); // update the terminal screen
                        sleep(2);
                        printBoard(w, init_row, box_margin);               wrefresh( w ); // update the terminal screen
                    }
                    //mvwprintw( w, turn_row , 6,"%s", currentTurnBuffer);
                    wrefresh( w ); // update the terminal screen
                }
                // PLAYER LOST 
                else if(game_status == 0)
                {
                    werase(w);
                    mvwprintw(w, rows / 2, (cols / 2) - (strlen("YOU LOST!") / 2 + 1), "YOU LOST!");
                    wrefresh( w ); // update the terminal screen
                    sleep(3);
                    game_status = 0;
                    bzero(buffer, BUFFER_SIZE);
                    sprintf(buffer, "%d %d %d %c", game_status, turn_status, my_turn, send_char);
                    sendString(connection_fd, buffer, strlen(buffer)+1);
                    break;
                }
                // PLAYER WON THE GAME WITH GAME STATUS 2
                else
                {
                    werase(w);
                    mvwprintw(w, rows / 2, (cols / 2) - (strlen("YOU WON THE GAME!!!") / 2 + 1), "YOU WON THE GAME!!!");
                    wrefresh( w ); // update the terminal screen
                    sleep(3);
                    game_status = 0;
                    bzero(buffer, BUFFER_SIZE);
                    sprintf(buffer, "%d %d %d %c", game_status, turn_status, my_turn, send_char);
                    sendString(connection_fd, buffer, strlen(buffer)+1);
                    break;
                }
            }
        }
    }
    
    delwin( w );
    endwin();
}


