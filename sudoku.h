/**
 * Compile-time options for the game of Sudoku.
 */

#define AUTHOR "cs50"
#define TITLE "Sudoku"

// Banner's colours.
#define FG_BANNER COLOR_CYAN
#define BG_BANNER COLOR_BLACK

// Grid's colours.
#define FG_GRID COLOR_WHITE
#define BG_GRID COLOR_BLACK

// Border's colours.
#define FG_BORDER COLOR_WHITE
#define BG_BORDER COLOR_RED

// Logo's colours.
#define FG_LOGO COLOR_WHITE
#define BG_LOGO COLOR_BLACK

// Solved grid's colours.
#define FG_SOLVED COLOR_GREEN
#define BG_SOLVED COLOR_BLACK

// Invalid row/column/box colours.
#define FG_INVALID COLOR_RED
#define BG_INVALID COLOR_BLACK

// Colour pairs.
enum { PAIR_BANNER = 1, PAIR_GRID, PAIR_BORDER, PAIR_LOGO, PAIR_SOLVED,
       PAIR_INVALID };

