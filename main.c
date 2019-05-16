#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <ncurses.h>
#include <string.h>
 
int main() {

    struct winsize win;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &win);

    int margin = 2;
    //printf ("lines %d\n", win.ws_row);
    //printf ("columns %d\n", win.ws_col);
    int cols = win.ws_col - (margin * 2);
    int rows = win.ws_row - (margin * 2);

    
    WINDOW *w;
    char list[5][7] = { "One", "Two", "Three", "Four", "Five" };
    char item[7];
    int ch, i = 0, width = 7;
 
    initscr(); // initialize Ncurses
    w = newwin( rows, cols, margin, margin ); // create a new window
    box( w, 0, 0 ); // sets default borders for the window
    char player[1024];

    bzero(player, 1024);
    sprintf(player, "Player %d", 1);

    int box_margin = cols / 5;
    int init_row = 4;
    int start_name = ((cols/2) + margin) - (strlen(player)/2) - 1;
    int start_chars = ((cols/2) + margin) - box_margin - 1;
    mvwprintw( w, 1, start_name, "%s", player );

    int ascii = 97;
    for (int row = 0; row < 4; row++)
    {
        for (int col = 1; col < 5; col++)
        {
            mvwprintw( w, init_row + (2 * row) , box_margin * col, "%c", ascii );
            ascii++;
        }
    }

    // now print all the menu items and highlight the first one
    //for( i=0; i<5; i++ ) {
    //    if( i == 0 ) 
    //        wattron( w, A_STANDOUT ); // highlights the first item.
    //    else
    //        wattroff( w, A_STANDOUT );
    //    sprintf(item, "%-7s",  list[i]);
    //    mvwprintw( w, i+1, 2, "%s", item );
    //}
 
    wrefresh( w ); // update the terminal screen
 
    i = 0;
    noecho(); // disable echoing of characters on the screen
    keypad( w, TRUE ); // enable keyboard input for the window.
    curs_set( 0 ); // hide the default screen cursor.
     
       // get the input
    while(( ch = wgetch(w)) != 'q'){ 
         
                // right pad with spaces to make the items appear with even width.
            sprintf(item, "%-7s",  list[i]); 
            mvwprintw( w, i+1, 2, "%s", item ); 
              // use a variable to increment or decrement the value based on the input.
            switch( ch ) {
                case KEY_UP:
                            i--;
                            i = ( i<0 ) ? 4 : i;
                            break;
                case KEY_DOWN:
                            i++;
                            i = ( i>4 ) ? 0 : i;
                            break;
            }
            // now highlight the next item in the list.
            wattron( w, A_STANDOUT );
             
            sprintf(item, "%-7s",  list[i]);
            mvwprintw( w, i+1, 2, "%s", item);
            wattroff( w, A_STANDOUT );
    }
 
    delwin( w );
    endwin();
    
    return 0;
}