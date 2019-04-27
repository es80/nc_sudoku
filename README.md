# CS50 ncurses Sudoku

Implements the game of sudoku. This program is a solution to an
[old problem set](https://cdn.cs50.net/2011/fall/psets/4/hacker4.pdf) from the
Harvard edX
[CS50 Introduction to Computer Science course](https://www.edx.org/course/cs50s-introduction-to-computer-science). The skeleton code can be found
[here](https://cdn.cs50.net/2011/fall/psets/4/hacker4.zip).

The features added include a solver which can provide error checking and hints,
an optional timer and undo/redo options.

## Instructions

To make and run locally requires

```
gcc
make
ncurses
```

then to play a game

```
make
./sudoku n00b|l33t [#]
```

Option `n00b` loads a set of 1024 easy puzzles. Option `l33t` loads a set of
1024 harder puzzles. Adding a number will load that specific puzzle number,
leaving it out will load a random puzzle from the set.

The arrow keys move the cursor around the grid. Enter digits using 1-9, and
erase a mistake with 0, full-stop or backspace.

To check progress use 'c', to get a hint press 'h'. Press 't' to toggle display
of a timer, and 'u' and 'ctrl-r' to undo and redo moves.

To start a new random puzzle use 'n', restart the current puzzle with 'r'.

### Screenshot

![CS50 ncurses Sudoku screenshot](/sudoku_screenshot.png?raw=true)

