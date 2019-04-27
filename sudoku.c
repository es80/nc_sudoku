/**
 * sudoku.c
 *
 * Implements the game of Sudoku. This program is a solution to an old problem
 * set from the Harvard edX CS50 Introduction to Computer Science course,
 * https://cdn.cs50.net/2011/fall/psets/4/hacker4.pdf
 *
 * The skeleton code can be found at
 * https://cdn.cs50.net/2011/fall/psets/4/hacker4.zip
 *
 * The features added include a solver which can provide error checking and
 * hints, an optional timer and undo/redo options.
 */

#include "sudoku.h"

#include <ctype.h>
#include <ncurses.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Macro for processing control characters.
#define CTRL(x) ((x) & ~0140)

// Alternative backspace.
#define ALT_KEY_BACKSPACE 127

// Size of each int (in bytes) in *.bin files.
#define INTSIZE 4

// Stack for undo/redo feature.
typedef struct stack
{
    // The board location.
    int y, x;
    // The number replaced.
    int replaced;
    // Pointer to next node in stack.
    struct stack *next;
}
stack;

// Various states that the board might be in, used to display messages.
enum state { BOARD_OK, INVALID_PLACEMENT, INVALID_BOARD, WON, CHECK, BAD_CHECK,
             HINT, FIX_HINT };

// Wrapper for game's globals.
struct
{
    // The current level.
    char *level;

    // The board's number.
    int number;

    // The board's top-left coordinates.
    int top, left;

    // The cursor's current location between (0,0) and (8,8).
    int y, x;

    // The game's current board.
    int board[9][9];

    // The game's starting board.
    int start_board[9][9];

    // A flag for solving the puzzle and a board for storing the solution.
    bool solved;
    int solved_board[9][9];

    // Stacks for undo/redo feature.
    stack *undo, *redo;

    // Times for start and end of game.
    time_t start, end;

    // Switch for showing timer.
    bool timer_showing;

    // The current state of the board used to display a message.
    enum state board_state;
}
g;

// Function prototypes.

// Functions for determining whether the board is in a valid state or solved.
bool valid_placement(int y, int x);
bool valid_row(int row);
bool valid_column(int column);
bool valid_box(int box);
bool valid_board(void);
bool is_won(void);

// Function for brute force solving the puzzle and storing the solution and
// functions for hint and check features.
void backtracking(void);
bool check(void);
bool get_hint(void);

// Functions for drawing permanent features in the window.
void draw_borders(void);
void draw_logo(void);
void draw_grid(void);
void draw_numbers(void);
void show_cursor(void);
void redraw_all(void);

// Functions for drawing temporary features in the window.
void show_banner(char *b);
void hide_banner(void);
void update_banner(void);
void show_timer(double elapsed);
void hide_timer(void);

// Functions for stack operations, used for undo/redo feature.
void pop(stack **ptr);
void push(stack **ptr, int y, int x, int replaced);
void clear_stack(stack **ptr);

// Functions for starting/ending ncurses, loading and (re)starting games and
// changing window size.
bool startup(void);
bool load_board(void);
bool restart_game(void);
void handle_signal(int signum);


int main(int argc, char *argv[])
{
    // Check usage.
    const char *usage = "Usage: sudoku n00b|l33t [#]\n";
    if (argc != 2 && argc != 3)
    {
        fprintf(stderr, usage);
        return 1;
    }

    // Ensure that level is valid.
    if (strcmp(argv[1], "debug") == 0)
        g.level = "debug";
    else if (strcmp(argv[1], "n00b") == 0)
        g.level = "n00b";
    else if (strcmp(argv[1], "l33t") == 0)
        g.level = "l33t";
    else
    {
        fprintf(stderr, usage);
        return 2;
    }

    // n00b and l33t levels have 1024 boards; debug level has 9.
    int max = (strcmp(g.level, "debug") == 0) ? 9 : 1024;

    if (argc == 3)
    {
        // Ensure n is integral.
        char c;
        if (sscanf(argv[2], " %d %c", &g.number, &c) != 1)
        {
            fprintf(stderr, usage);
            return 3;
        }

        // Ensure n is in [1, max].
        if (g.number < 1 || g.number > max)
        {
            fprintf(stderr, "That board # does not exist!\n");
            return 4;
        }

        // Seed PRNG with # so that we get same sequence of boards.
        srand(g.number);
    }
    else
    {
        // Seed PRNG with current time so that we get any sequence of boards.
        srand(time(NULL));

        // Choose a random n in [1, max].
        g.number = rand() % max + 1;
    }

    // Start up ncurses.
    if (!startup())
    {
        fprintf(stderr, "Error starting up ncurses!\n");
        return 5;
    }

    // Register handler for SIGWINCH (SIGnal WINdow CHanged).
    signal(SIGWINCH, (void (*)(int)) handle_signal);

    // Start the first game.
    if (!restart_game())
    {
        endwin();
        fprintf(stderr, "Could not load board from disk!\n");
        return 6;
    }
    redraw_all();

    // Game loop.
    int ch;
    do
    {
        // Refresh the screen.
        refresh();

        // Get user's input and capitalise.
        ch = getch();
        ch = toupper(ch);

        switch (ch)
        {
            // Start a new game.
            case 'N':
                g.number = rand() % max + 1;
                if (!restart_game())
                {
                    endwin();
                    fprintf(stderr, "Could not load board from disk!\n");
                    return 6;
                }
                break;

            // Restart current game.
            case 'R':
                if (!restart_game())
                {
                    endwin();
                    fprintf(stderr, "Could not load board from disk!\n");
                    return 6;
                }
                break;

            // Let user manually redraw screen with ctrl-L.
            case CTRL('l'):
                redraw_all();
                break;

            // Move the cursor with keypad.
            case KEY_LEFT:
                g.x = (g.x + 8) % 9;
                break;

            case KEY_RIGHT:
                g.x = (g.x + 10) % 9;
                break;

            case KEY_UP:
                g.y = (g.y + 8) % 9;
                break;

            case KEY_DOWN:
                g.y = (g.y + 10) % 9;
                break;

            // Enter a number.
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                // Don't allow changes to starting numbers, nor if won already.
                if (g.board_state != WON && g.start_board[g.y][g.x] == 0)
                {
                    // Store the change for undo.
                    push(&g.undo, g.y, g.x, g.board[g.y][g.x]);

                    // Redo doesn't branch so must be cleared.
                    clear_stack(&g.redo);

                    // Print the number and update board.
                    addch(ch);
                    g.board[g.y][g.x] = ch - '0';

                    // Update the state of the board.
                    if (!valid_placement(g.y, g.x))
                    {
                        g.board_state = INVALID_PLACEMENT;
                    }
                    else if (!valid_board())
                    {
                        g.board_state = INVALID_BOARD;
                    }
                    else if (is_won())
                    {
                        g.board_state = WON;
                        // Stop the timer.
                        time(&g.end);
                    }
                    else
                    {
                        g.board_state = BOARD_OK;
                    }

                    // Change banner and colour numbers.
                    update_banner();
                    draw_numbers();
                }
                break;

            // Remove a number.
            case '0':
            case KEY_DC:
            case KEY_BACKSPACE:
            case ALT_KEY_BACKSPACE:
            case '.':
                // Don't allow changes to starting numbers, nor if won already.
                if (g.board_state != WON && g.start_board[g.y][g.x] == 0)
                {
                    // Store the change for undo.
                    push(&g.undo, g.y, g.x, g.board[g.y][g.x]);

                    // Redo doesn't branch so must be cleared.
                    clear_stack(&g.redo);

                    // Print the 'empty' and update board.
                    addch('.');
                    g.board[g.y][g.x] = 0;

                    // Update the state of the board.
                    if (!valid_board())
                    {
                        g.board_state = INVALID_BOARD;
                    }
                    else
                    {
                        g.board_state = BOARD_OK;
                    }

                    // Change banner and colour numbers.
                    update_banner();
                    draw_numbers();
                }
                break;

            // Undo changes to the board
            case 'U':
            case CTRL('Z'):
                // Check puzzle is not won and there exist moves to undo.
                if (g.board_state != WON && g.undo)
                {
                    g.x = g.undo->x;
                    g.y = g.undo->y;

                    // Store the move in redo stack.
                    push(&g.redo, g.y, g.x, g.board[g.y][g.x]);

                    // Update the board and pop move from undo stack.
                    g.board[g.y][g.x] = g.undo->replaced;
                    pop(&g.undo);

                    // Update the state of the board.
                    if (!valid_board())
                    {
                        g.board_state = INVALID_BOARD;
                    }
                    // If undoing to satisfy check, continue to display message.
                    else if (g.board_state == BAD_CHECK && !check())
                    {
                        g.board_state = BAD_CHECK;
                    }
                    else
                    {
                        g.board_state = BOARD_OK;
                    }

                    // Change banner and colour numbers.
                    update_banner();
                    draw_numbers();
                }
                break;

            // Redo changes to the board.
            case CTRL('r'):
                // Check we have moves to redo.
                if (g.redo)
                {
                    g.x = g.redo->x;
                    g.y = g.redo->y;

                    // Store the move in undo stack.
                    push(&g.undo, g.y, g.x, g.board[g.y][g.x]);

                    // Update board and pop move from redo stack.
                    g.board[g.y][g.x] = g.redo->replaced;
                    pop(&g.redo);

                    // Update the state of the board.
                    if (!valid_placement(g.y, g.x))
                    {
                        g.board_state = INVALID_PLACEMENT;
                    }
                    else if (!valid_board())
                    {
                        g.board_state = INVALID_BOARD;
                    }
                    else
                    {
                        g.board_state = BOARD_OK;
                    }

                    // Change banner and colour numbers.
                    update_banner();
                    draw_numbers();
                }
                break;

            // Show or hide the timer.
            case 'T':
                // Just change the flag here.
                g.timer_showing = 1 - g.timer_showing;
                break;

            // Check the cells filled so far are indeed correct.
            case 'C':
                if (g.board_state != WON)
                {
                    // If correct, 'save' the board.
                    if (check())
                    {
                        // Prevent undo/redo.
                        clear_stack(&g.undo);
                        clear_stack(&g.redo);

                        // Treat filled squares as the starting puzzle to
                        // change colour and prevent alteration.
                        memcpy(g.start_board, g.board, sizeof(g.board));

                        g.board_state = CHECK;

                        // Colour numbers.
                        draw_numbers();
                    }
                    // Else inform user of error.
                    else
                    {
                        g.board_state = BAD_CHECK;
                    }

                    update_banner();
                }
                break;

            // Provide hint.
            case 'H':
                if (g.board_state != WON)
                {
                    // Request a hint.
                    if (get_hint())
                    {
                        // Update the state of the board.
                        if (is_won())
                        {
                            g.board_state = WON;
                            // Stop the timer.
                            time(&g.end);
                        }
                        else
                        {
                            g.board_state = HINT;
                        }
                    }
                    else
                    {
                        // Correct the mistakes using undos.
                        while (!check())
                        {
                            g.x = g.undo->x;
                            g.y = g.undo->y;
                            push(&g.redo, g.y, g.x, g.board[g.y][g.x]);
                            g.board[g.y][g.x] = g.undo->replaced;
                            pop(&g.undo);
                        }
                        g.board_state = FIX_HINT;
                    }

                    // Change banner and colour numbers.
                    draw_numbers();
                    update_banner();
                }
                break;

        }

        // Restore the cursor and update the timer.
        if (g.board_state != WON)
        {
            if (g.timer_showing)
            {
                show_timer(difftime(time(NULL), g.start));
            }
            else
            {
                hide_timer();
            }
            show_cursor();
        }
        else
        {
            if (g.timer_showing)
            {
                show_timer(difftime(g.end, g.start));
            }
            else
            {
                hide_timer();
            }
            // If game won hide cursor.
            curs_set(0);
        }
    }
    while (ch != 'Q');

    // Shut down ncurses.
    endwin();

    // Clear undo and redo stacks.
    clear_stack(&g.undo);
    clear_stack(&g.redo);

    // Tidy up the screen (using ANSI escape sequences).
    printf("\033[2J");
    printf("\033[%d;%dH", 0, 0);

    return 0;
}

/*
 * Returns true iff the number the user placed row y and column x is a valid
 * placement, i.e. that particular number appears only once in the
 * corresponding row, column and box.
 */
bool valid_placement(int y, int x)
{
    // Check the row and column containing (y,x).
    for (int i = 0; i < 9; i++)
    {
        if (i != x && g.board[y][i] == g.board[y][x])
        {
            return false;
        }
        if (i != y && g.board[i][x] == g.board[y][x])
        {
            return false;
        }
    }

    // Calculate the co-ordinates of the top left of box containing (y,x).
    int box_y = y - (y % 3);
    int box_x = x - (x % 3);

    // Check the rows and columns of the box.
    for (int i = box_y; i < 3; i++)
    {
        for (int j = box_x; j < 3; j++)
        {
            if (i != y && j != x && g.board[i][j] == g.board[y][x])
            {
                return false;
            }
        }
    }

    return true;
}

/*
 * Returns true iff the given row is currently valid, i.e. each number occurs
 * once, or not at all, in the column.
 */
bool valid_row(int row)
{
    // Array to count occurrences of numbers 1-9.
    int counts[9] = {0};

    // Check each column of the row.
    for (int col = 0; col < 9; col++)
    {
        if (g.board[row][col])
        {
            if (counts[g.board[row][col] - 1] == 1)
            {
                // Oops, already seen this number once.
                return false;
            }
            else
            {
                counts[g.board[row][col] - 1]++;
            }
        }
    }
    return true;
}

/*
 * Returns true iff the given column is currently valid, i.e. each number
 * occurs once, or not at all, in the column.
 */
bool valid_column(int column)
{
    // Array to count occurrences of numbers 1-9.
    int counts[9] = {0};

    // Check each row of the column.
    for (int row = 0; row < 9; row++)
    {
        if (g.board[row][column])
        {
            if (counts[g.board[row][column] - 1] == 1)
            {
                // Oops, already seen this number once.
                return false;
            }
            else
            {
                counts[g.board[row][column] - 1]++;
            }
        }
    }
    return true;
}

/*
 * Returns true iff the given box is currently valid, i.e. each number occurs
 * once, or not at all in the 3x3 box. Boxes are numbered 0-8, top-to-bottom
 * then left-to-right.
 */
bool valid_box(int box)
{
    // Array to count occurrences of numbers 1-9.
    int counts[9] = {0};

    // Check the rows and columns of the box.
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            // Calculate the rows and columns for the box.
            int row = i + 3 * (box % 3);
            int col = j + 3 * (box / 3);
            if (g.board[row][col])
            {
                if (counts[g.board[row][col] - 1] == 1)
                {
                    // Oops, already seen this number once.
                    return false;
                }
                else
                {
                    counts[g.board[row][col] - 1]++;
                }
            }
        }
    }
    return true;
}

/*
 * Returns true iff the whole board is currently valid, i.e. each number occurs
 * at once, or not at all, in each row, column and box.
 */
bool valid_board(void)
{
    for (int i = 0; i < 9; i++)
    {
        if (!(valid_row(i) && valid_column(i) && valid_box(i)))
        {
            return false;
        }
    }
    return true;
}

/*
 * Returns true iff the puzzle is solved.
 */
bool is_won(void)
{
    // Check no unfilled locations.
    for (int i = 0; i < 9; i++)
    {
        for (int j = 0; j < 9; j++)
        {
            if (g.board[i][j] == 0)
            {
                return false;
            }
        }
    }
    // If the board is valid and has no unfilled locations, it is solved.
    return valid_board();
}

/*
 * Recursively solves the puzzle in g.board using backtracking trial and error.
 */
void backtracking(void)
{
    // If the board is invalid we can go back.
    if (!valid_board())
    {
        return;
    }
    // If the board is solved, note the solution.
    else if (is_won())
    {
        // Use a flag to break out of the recursion.
        g.solved = true;

        // Copy solution into memory for game's solved board.
        memcpy(g.solved_board, g.board, sizeof(g.board));

        // Restore the starting unsolved state of the board.
        memcpy(g.board, g.start_board, sizeof(g.start_board));

        return;
    }

    // Search for the first blank square.
    int c_row = 0;
    int c_col = 0;

    for (int row = 0; row < 9; row++)
    {
        for (int col = 0; col < 9; col++)
        {
            if (g.board[row][col] == 0)
            {
                c_row = row;
                c_col = col;
            }
        }
    }

    // For this square, try all nine numbers as candidates.
    for (int i = 1; i <= 9; i++)
    {
        g.board[c_row][c_col] = i;

        // Test this candidate further.
        backtracking();

        // If puzzle now solved we can exit the loop.
        if (g.solved)
        {
            break;
        }
    }

    // If the puzzle is not solved, none of the candidates worked so we must
    // reset that square to 0 and backtrack.
    if (!g.solved)
    {
        g.board[c_row][c_col] = 0;
    }
}

/*
 * Returns true if the numbers currently on the board are correct according to
 * the solution.
 */
bool check(void)
{
    for (int row = 0; row < 9; row++)
    {
        for (int col = 0; col < 9; col++)
        {
            // If a square contains a number but it does not match solution.
            if (g.board[row][col] &&
                g.board[row][col] != g.solved_board[row][col])
            {
                return false;
            }
        }
    }
    return true;
}

/*
 * Returns true iff a hint is provided. If the board currently has a mistake
 * returns false. Otherwise returns true having randomly selected an empty
 * square from the board and filled it with using the solution.
 */
bool get_hint(void)
{
    // If the board currently has an error, the hint feature will undo it.
    if (!check())
    {
        return false;
    }
    // Otherwise provide a hint.
    else
    {
        // Count the number of empty squares.
        int empty_squares = 0;
        for (int row = 0; row < 9; row++)
        {
            for (int col = 0; col < 9; col++)
            {
                if (!g.board[row][col])
                {
                    empty_squares++;
                }
            }
        }

        // Choose a random location in [0, empty_cells - 1].
        int target = rand() % empty_squares;

        // Go to that location.
        empty_squares = 0;
        for (int row = 0; row < 9; row++)
        {
            for (int col = 0; col < 9; col++)
            {
                if (!g.board[row][col] && empty_squares == target)
                {
                    // Insert the number from the solution.
                    g.board[row][col] = g.solved_board[row][col];
                    // Prepare to move cursor to square.
                    g.y = row;
                    g.x = col;
                    // Break the loop.
                    row = 9;
                    break;
                }
                else if (!g.board[row][col])
                {
                    empty_squares++;
                }
            }
        }
    }
    return true;
}

/*
 * Draws game's borders.
 */
void draw_borders(void)
{
    // Get window's dimensions.
    int maxy, maxx;
    getmaxyx(stdscr, maxy, maxx);

    // Enable colour if possible (else b&w highlighting).
    if (has_colors())
    {
        attron(A_PROTECT);
        attron(COLOR_PAIR(PAIR_BORDER));
    }
    else
        attron(A_REVERSE);

    // Draw borders.
    for (int i = 0; i < maxx; i++)
    {
        mvaddch(0, i, ' ');
        mvaddch(maxy-1, i, ' ');
    }

    // Draw header.
    char header[maxx+1];
    sprintf(header, "%s by %s", TITLE, AUTHOR);
    mvaddstr(0, (maxx - strlen(header)) / 2, header);

    // Draw footer.
    mvaddstr(maxy-1, 1, "[N]ew Game   [R]estart Game   [T]imer show/hide   "
                        "[U]ndo   [Ctrl-R]edo   [C]heck   [H]int");
    mvaddstr(maxy-1, maxx-13, "[Q]uit Game");

    // Disable colour if possible (else b&w highlighting).
    if (has_colors())
        attroff(COLOR_PAIR(PAIR_BORDER));
    else
        attroff(A_REVERSE);
}

/*
 * Draws game's logo.  Must be called after draw_grid has been
 * called at least once.
 */
void draw_logo(void)
{
    // Determine top-left coordinates of logo.
    int top = g.top + 2;
    int left = g.left + 30;

    // Enable colour if possible.
    if (has_colors())
        attron(COLOR_PAIR(PAIR_LOGO));

    // Draw logo.
    mvaddstr(top + 0, left, "               _       _          ");
    mvaddstr(top + 1, left, "              | |     | |         ");
    mvaddstr(top + 2, left, " ___ _   _  __| | ___ | | ___   _ ");
    mvaddstr(top + 3, left, "/ __| | | |/ _` |/ _ \\| |/ / | | |");
    mvaddstr(top + 4, left, "\\__ \\ |_| | (_| | (_) |   <| |_| |");
    mvaddstr(top + 5, left, "|___/\\__,_|\\__,_|\\___/|_|\\_\\\\__,_|");

    // Sign logo.
    char signature[3+strlen(AUTHOR)+1];
    sprintf(signature, "by %s", AUTHOR);
    mvaddstr(top + 7, left + 35 - strlen(signature) - 1, signature);

    // Disable colour if possible.
    if (has_colors())
        attroff(COLOR_PAIR(PAIR_LOGO));
}

/*
 * Draw's the game's board.
 */
void draw_grid(void)
{
    // Get window's dimensions.
    int maxy, maxx;
    getmaxyx(stdscr, maxy, maxx);

    // Determine where top-left corner of board belongs.
    g.top = maxy/2 - 7;
    g.left = maxx/2 - 30;

    // Enable colour if possible.
    if (has_colors())
        attron(COLOR_PAIR(PAIR_GRID));

    // Print grid.
    for (int i = 0 ; i < 3 ; ++i )
    {
        mvaddstr(g.top + 0 + 4 * i, g.left, "+-------+-------+-------+");
        mvaddstr(g.top + 1 + 4 * i, g.left, "|       |       |       |");
        mvaddstr(g.top + 2 + 4 * i, g.left, "|       |       |       |");
        mvaddstr(g.top + 3 + 4 * i, g.left, "|       |       |       |");
    }
    mvaddstr(g.top + 4 * 3, g.left, "+-------+-------+-------+" );

    // Remind user of level and #.
    char reminder[maxx+1];
    sprintf(reminder, "   playing %s #%d", g.level, g.number);
    mvaddstr(g.top + 14, g.left + 25 - strlen(reminder), reminder);

    // Disable colour if possible.
    if (has_colors())
        attroff(COLOR_PAIR(PAIR_GRID));
}

/*
 * Draw's game's numbers.  Must be called after draw_grid has been
 * called at least once. Uses up to four colours depending on whether the
 * puzzle is solved, the numbers are those from the start of the puzzle, or
 * those the user has added or those within an invalid row, column or box.
 */
void draw_numbers(void)
{
    // Have different colours for completed puzzle.
    int colours = g.board_state == WON ? PAIR_SOLVED : PAIR_BANNER;

    // Iterate over board's numbers.
    for (int i = 0; i < 9; i++)
    {
        for (int j = 0; j < 9; j++)
        {
            // Determine char.
            char c = (g.board[i][j] == 0) ? '.' : g.board[i][j] + '0';

            // If we have a completed game or the number was given at the start
            // of the puzzle.
            if (colours == PAIR_SOLVED ||
                (g.board[i][j] && g.board[i][j] == g.start_board[i][j]))
            {
                // Enable colour if possible.
                if (has_colors())
                    attron(COLOR_PAIR(colours));

                // Add char to window.
                mvaddch(g.top + i + 1 + i/3, g.left + 2 + 2*(j + j/3), c);

                // Disable colour if possible.
                if (has_colors())
                    attroff(COLOR_PAIR(colours));
            }

            // Otherwise use default colours.
            else
            {
                mvaddch(g.top + i + 1 + i/3, g.left + 2 + 2*(j + j/3), c);
            }
            refresh();
        }
    }

    // Now determine colouring, if any, for invalid parts of puzzle.
    // Enable colour if possible.
    if (has_colors())
        attron(COLOR_PAIR(PAIR_INVALID));

    for (int i = 0; i < 9; i++)
    {
        if (!valid_row(i))
        {
            for (int j = 0; j < 9; j++)
            {
                // Determine char.
                char c = (g.board[i][j] == 0) ? '.' : g.board[i][j] + '0';
                // Add char to window.
                mvaddch(g.top + i + 1 + i/3, g.left + 2 + 2*(j + j/3), c);
                refresh();
            }
        }
        if (!valid_column(i))
        {
            for (int j = 0; j < 9; j++)
            {
                // Determine char.
                char c = (g.board[j][i] == 0) ? '.' : g.board[j][i] + '0';
                // Add char to window.
                mvaddch(g.top + j + 1 + j/3, g.left + 2 + 2*(i + i/3), c);
                refresh();
            }
        }
        if (!valid_box(i))
        {
            for (int j = 0; j < 3; j++)
            {
                for (int k = 0; k < 3; k++)
                {
                    // Calculate the rows and columns for the box.
                    int row = j + 3 * (i % 3);
                    int col = k + 3 * (i / 3);
                    // Determine char.
                    char c = (g.board[row][col] == 0) ?
                              '.' : g.board[row][col] + '0';
                    // Add char to window.
                    mvaddch(g.top + row + 1 + row/3,
                            g.left + 2 + 2*(col + col/3), c);
                    refresh();
                }
            }
        }
    }

    // Disable colour if possible.
    if (has_colors())
        attroff(COLOR_PAIR(PAIR_INVALID));
}

/*
 * Shows cursor at (g.y, g.x).
 */
void show_cursor(void)
{
    // Restore cursor's location.
    move(g.top + g.y + 1 + g.y/3, g.left + 2 + 2*(g.x + g.x/3));
}

/*
 * (Re)draws everything on the screen except timer and banner.
 */
void redraw_all(void)
{
    // Reset ncurses.
    endwin();
    refresh();

    // Clear screen.
    clear();

    // Re-draw everything.
    draw_borders();
    draw_grid();
    draw_logo();
    draw_numbers();
    update_banner();
    show_cursor();
}

/*
 * Shows a banner. Must be called after show_grid has been called at least
 * once.
 */
void show_banner(char *b)
{
    // Enable colour if possible.
    if (has_colors())
        attron(COLOR_PAIR(PAIR_BANNER));

    // Determine location from top-left corner of board.
    mvaddstr(g.top + 16, g.left + 64 - strlen(b), b);

    // Disable colour if possible.
    if (has_colors())
        attroff(COLOR_PAIR(PAIR_BANNER));
}

/*
 * Hides banner.
 */
void hide_banner(void)
{
    // Get window's dimensions.
    int maxy, maxx;
    getmaxyx(stdscr, maxy, maxx);

    // Overwrite banner with spaces.
    for (int i = 0; i < maxx; i++)
        mvaddch(g.top + 16, i, ' ');
}

/*
 * Updates banner to inform user. Called if any change to the board state is
 * made.
 */
void update_banner(void)
{
    hide_banner();

    // Display a different message to the user depending only on g.board_state.
    switch (g.board_state)
    {
        case BOARD_OK:
            return;

        case INVALID_PLACEMENT:
            show_banner("Oops! That number can't go there. "
                        "Use 'u' to undo moves.");
            break;

        case INVALID_BOARD:
            show_banner("Oops! There's still a problem somewhere. "
                        "Use 'u' to undo moves.");
            break;

        case WON:
            show_banner("Congratulations! You solved the puzzle!");
            break;

        case CHECK:
            show_banner("So far, so good...");
            break;

        case BAD_CHECK:
            show_banner("Oops! You've made a mistake somewhere. "
                        "Use 'u' to undo moves or 'h' to fix.");
            break;

        case HINT:
            show_banner("Hope that helps!");
            break;

        case FIX_HINT:
            show_banner("Any mistakes are now fixed!");
            break;
    }
}

/*
 * Shows a timer with the number of seconds since the puzzle was started.
 */
void show_timer(double elapsed)
{
    // Enable colour if possible.
    if (has_colors())
        attron(COLOR_PAIR(PAIR_INVALID));

    char time_string[18];
    sprintf(time_string, "time: %d", (int) elapsed);

    // Determine location from top-left corner of board.
    mvaddstr(g.top + 14, g.left + 64 - strlen(time_string), time_string);

    // Disable colour if possible.
    if (has_colors())
        attroff(COLOR_PAIR(PAIR_INVALID));
}

/*
 * Hides timer.
 */
void hide_timer(void)
{
    // Get window's dimensions.
    int maxy, maxx;
    getmaxyx(stdscr, maxy, maxx);

    // Overwrite timer with spaces.
    for (int i = g.left + 26; i < maxx; i++)
        mvaddch(g.top + 14, i, ' ');
}

/*
 * Pop from a stack and free the memory for that node.
 */
void pop(stack **ptr)
{
    if (!*ptr)
    {
        return;
    }
    else
    {
        stack *head = *ptr;
        *ptr = head->next;
        free(head);
    }
}

/*
 * Push to a stack, allocates new memory for a node.
 */
void push(stack **ptr, int y, int x, int replaced)
{
    stack *new_ptr = malloc(sizeof(stack));
    if (!new_ptr)
    {
        return;
    }
    else
    {
        new_ptr->y = y;
        new_ptr->x = x;
        new_ptr->replaced = replaced;
        new_ptr->next = *ptr;
        *ptr = new_ptr;
    }
}

/*
 * Clears stack by popping all its members and setting stack pointer to NULL.
 */
void clear_stack(stack **ptr)
{
    if (*ptr)
    {
        while (*ptr)
        {
            pop(ptr);
        }
    }
    *ptr = NULL;
}

/*
 * Starts up ncurses.  Returns true iff successful.
 */
bool startup(void)
{
    // Initialize ncurses.
    if (initscr() == NULL)
    {
        return false;
    }

    // Prepare for colour if possible.
    if (has_colors())
    {
        // Enable colour.
        if (start_color() == ERR || attron(A_PROTECT) == ERR)
        {
            endwin();
            return false;
        }

        // Initialize pairs of colours.
        if (init_pair(PAIR_BANNER, FG_BANNER, BG_BANNER) == ERR ||
            init_pair(PAIR_GRID, FG_GRID, BG_GRID) == ERR ||
            init_pair(PAIR_BORDER, FG_BORDER, BG_BORDER) == ERR ||
            init_pair(PAIR_LOGO, FG_LOGO, BG_LOGO) == ERR ||
            init_pair(PAIR_SOLVED, FG_SOLVED, BG_SOLVED) == ERR ||
            init_pair(PAIR_INVALID, FG_INVALID, BG_INVALID) == ERR)
        {
            endwin();
            return false;
        }
    }

    // Don't echo keyboard input.
    if (noecho() == ERR)
    {
        endwin();
        return false;
    }

    // Disable line buffering, allow ctrl-C signal.
    if (cbreak() == ERR)
    {
        endwin();
        return false;
    }

    // Enable arrow keys.
    if (keypad(stdscr, true) == ERR)
    {
        endwin();
        return false;
    }

    // Wait 100 ms at a time for input and regularity of timer.
    timeout(100);

    return true;
}

/*
 * Loads current board from disk, returning true iff successful.
 */
bool load_board(void)
{
    // Open file with boards of specified level.
    char filename[strlen(g.level) + 5];
    sprintf(filename, "%s.bin", g.level);
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL)
        return false;

    // Determine file's size.
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);

    // Ensure file is of expected size.
    if (size % (81 * INTSIZE) != 0)
    {
        fclose(fp);
        return false;
    }

    // Compute offset of specified board.
    int offset = ((g.number - 1) * 81 * INTSIZE);

    // Seek to specified board.
    fseek(fp, offset, SEEK_SET);

    // Read board into memory for game's current board.
    if (fread(g.board, 81 * INTSIZE, 1, fp) != 1)
    {
        fclose(fp);
        return false;
    }

    // Copy board into memory for game's starting board.
    memcpy(g.start_board, g.board, sizeof(g.board));

    fclose(fp);
    return true;
}

/*
 * (Re)starts current game, returning true iff succesful.
 */
bool restart_game(void)
{
    // Reload current game.
    if (!load_board())
    {
        return false;
    }

    // Clear undo and redo stacks.
    clear_stack(&g.undo);
    clear_stack(&g.redo);

    // Solve the puzzle for hint feature.
    g.solved = false;
    backtracking();

    // Reset timer and board_state.
    time(&g.start);
    g.timer_showing = true;
    g.board_state = BOARD_OK;

    // Redraw board.
    draw_grid();
    draw_numbers();
    hide_timer();
    hide_banner();
    curs_set(1);

    // Move cursor to board's center.
    g.y = g.x = 4;
    show_cursor();

    return true;
}

/*
 * Designed to handles signals (e.g., SIGWINCH).
 */
void handle_signal(int signum)
{
    // Handle a change in the window (i.e., a resizing).
    if (signum == SIGWINCH)
        redraw_all();

    // Re-register myself so this signal gets handled in future too.
    signal(signum, (void (*)(int)) handle_signal);
}

