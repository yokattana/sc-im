/*
main_win: window that loads the spreadsheetssword:

    input_win: stdin and state bar window
*/
#include <string.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <locale.h>
#include "input.h"
#include "screen.h"
#include "range.h"
#include "sc.h"
#include "cmds.h"
#include "cmds_visual.h"
#include "color.h"
#include "conf.h"
#include "version.h"
#include "file.h"
#include "format.h"
#include "utils/string.h"

extern struct dictionary * d_colors_param;
extern int cmd_pending;
extern int cmd_multiplier;
extern char insert_edit_submode;

unsigned int curmode;
int rescol = RESCOL;           // Columns reserved for row numbers

WINDOW * main_win;
WINDOW * input_win;

// off screen spreadsheet rows and columns
int offscr_sc_rows = 0, offscr_sc_cols = 0;

srange * ranges;

void start_screen() {
    initscr();

    main_win = newwin(LINES - RESROW, COLS, RESROW, 0);
    input_win = newwin(RESROW, COLS, 0, 0); // just 2 rows (RESROW = 2)

    #ifdef USECOLORS
    if (has_colors()) {
        start_color();

        if (get_d_colors_param() == NULL) {
            start_default_ucolors();

            // in case we decide to change colors
            // Create a dictionary and save equivalences between macros and
            // values defined in '.sc' files
            set_colors_param_dict();
        }
        wbkgd(main_win, COLOR_PAIR(ucolors[DEFAULT].fg * 8 + ucolors[DEFAULT].bg + 1));
        wbkgd(input_win, COLOR_PAIR(ucolors[DEFAULT].fg * 8 + ucolors[DEFAULT].bg + 1));
    }
    #endif

    wtimeout(input_win, TIMEOUT_CURSES);
    noecho();
    curs_set(0);

    #ifndef NETBSD
    if ((char *) getenv ("ESCDELAY") == NULL) set_escdelay(ESC_DELAY);
    #endif
    cbreak();
    keypad(input_win, 1);
}

void stop_screen() {
    #ifdef USECOLORS
        //if (get_d_colors_param() != NULL)
        free_colors_param_dict();
    #endif
    move(0, 0);
    clrtobot();
    refresh();

    endwin();
    return;
}

// Welcome screen
void do_welcome() {

    char * msg_title = "SC-IM - SpreadSheet Calculator Improvised";
    char * msg_by = "A SC fork by Andrés Martinelli";
    char * msg_version = rev;
    char * msg_help = "Press «:help<Enter>» to get help";

    #ifdef USECOLORS
    wbkgd(main_win, COLOR_PAIR(ucolors[DEFAULT].fg * 8 + ucolors[DEFAULT].bg + 1));
    wbkgd(input_win, COLOR_PAIR(ucolors[DEFAULT].fg * 8 + ucolors[DEFAULT].bg + 1));
    #endif

    // show headings
    int mxcol = offscr_sc_cols + calc_offscr_sc_cols() - 1;
    int mxrow = offscr_sc_rows + calc_offscr_sc_rows() - 1;
    show_sc_col_headings(main_win, mxcol, mxrow);
    show_sc_row_headings(main_win, mxrow);

    #ifdef USECOLORS
    set_ucolor(main_win, &ucolors[WELCOME]);
    #endif

    // show message
    mvwaddstr(main_win, LINES/2-2, COLS/2-strlen(msg_title)/2  , msg_title);
    mvwaddstr(main_win, LINES/2-1, COLS/2-strlen(msg_by)/2     , msg_by);
    mvwaddstr(main_win, LINES/2  , COLS/2-strlen(msg_version)/2, msg_version);
    mvwaddstr(main_win, LINES/2+2, COLS/2-strlen(msg_help)/2   , msg_help);

    wrefresh(main_win);

    return;
}


// function that refreshes grid of screen
void update(int header) {
    //#ifdef USECOLORS
    //wbkgd(main_win, COLOR_PAIR(ucolors[DEFAULT].fg * 8 + ucolors[DEFAULT].bg + 1));
    //wbkgd(input_win, COLOR_PAIR(ucolors[DEFAULT].fg * 8 + ucolors[DEFAULT].bg + 1));
    //#endif
    if (loading) return;
    if (cmd_multiplier > 1) return;
    if (atoi(get_conf_value("nocurses"))) return;

    // Clean from top to bottom
    if (header) {
        wmove(main_win, 0, rescol);
        wclrtobot(main_win);
    }

    // Calculate hidden rows and columns
    //   mxcol-1: las 'sc_col' in the screen
    //   mxrow-1: las 'sc_row' in the screen
    int off_cols = calc_offscr_sc_cols();
    int off_rows = calc_offscr_sc_rows();
    int mxcol = offscr_sc_cols + off_cols - 1;
    int mxrow = offscr_sc_rows + off_rows - 1;

    /* You can't hide the last row or col */
    while (row_hidden[currow])
        currow++;
    while (col_hidden[curcol])
        curcol++;

    // Show the content of the cells
    // Numeric values, strings.
    show_content(main_win, mxrow, mxcol);

    // Show sc_col headings: A, B, C, D..
    show_sc_col_headings(main_win, mxcol, mxrow);

    // Show sc_row headings: 0, 1, 2, 3..
    show_sc_row_headings(main_win, mxrow);

    // Refresh curses windows
    wrefresh(main_win);

    // Show cell details in header (first row)
    if (header)
        show_celldetails(input_win);

    // print mode
    (void) print_mode(input_win);

    return;
}

// Enable cursor and echo depending on the current mode
void handle_cursor() {
    switch (curmode) {
        case COMMAND_MODE:
            noecho();
            curs_set(1);
            break;
        case INSERT_MODE:
        case EDIT_MODE:
            noecho();
            curs_set(1); // changes for NETBSD compatibility
            break;
        default:
            noecho();
            curs_set(0);
    }
    return;
}

/* Print string with alignment
    JUSTIF: 0 left shift
    JUSTIF: 1 right shift */
void write_j(WINDOW * win, const char * word, const unsigned int row, const unsigned int justif) {
    (justif == 0) ? (wmove(win, row, 0) && wclrtoeol(win)) : wmove(win, row, COLS - strlen(word));
    wprintw(win, "%s", word);
    return;
}

// Print multiplier and pending operator on the status bar
void print_mult_pend(WINDOW * win) {

    if (curmode != NORMAL_MODE && curmode != VISUAL_MODE && curmode != EDIT_MODE) return;

    int row_orig, col_orig;
    getyx(win, row_orig, col_orig);

    #ifdef USECOLORS
    set_ucolor(win, &ucolors[MODE]);
    #endif
    // Show multiplier and pending operator
    char strm[COLS];
    strm[0]='\0';
    if (cmd_multiplier > 0) sprintf(strm, "%d", cmd_multiplier);
    if (cmd_pending) {
        strcat(strm, "?");
    }

    char field[rescol+1];
    field[0]='\0';
    sprintf(field, "%0*d", rescol - (int) strlen(strm), 0);
    subst(field, '0', ' ');
    strcat(strm, field);

    mvwprintw(win, 0, 0, "%s", strm);

    // Return cursor to previous position
    wmove(win, row_orig, col_orig);
}

// Show first and second row (header)
// Handle cursor position
void show_header(WINDOW * win) {

    clr_header(win, 0);
    clr_header(win, 1);

    print_mult_pend(win);

    // Show current mode
    print_mode(win);

    // Print input text
    #ifdef USECOLORS
    //OJO!!! 28/09/2016
    //set_ucolor(win, &ucolors[INPUT]);
    #endif
    switch (curmode) {
        case COMMAND_MODE:
            mvwprintw(win, 0, rescol, ":%ls", inputline);
            wmove(win, 0, inputline_pos + 1 + rescol);
            break;
        case INSERT_MODE:
            mvwprintw(win, 0, 1 + rescol, "%ls", inputline);
            wmove(win, 0, inputline_pos + 1 + rescol);
            break;
        case EDIT_MODE:
            mvwprintw(win, 0, rescol, " %ls", inputline);
            wmove(win, 0, inputline_pos + 1 + rescol);
    }
    wrefresh(win);

    return;
}

// Clean a whole row
void clr_header(WINDOW * win, int i) {
    int row_orig, col_orig;
    getyx(win, row_orig, col_orig);
    if (col_orig > COLS) col_orig = COLS - 1;

    wmove(win, i, 0);
    wclrtoeol(win);

    // Return cursor to previous position
    wmove(win, row_orig, col_orig);

    return;
}

// Print current mode in the first row
// Print ':' (colon) or submode indicator
void print_mode(WINDOW * win) {
    unsigned int row = 0; // Print mode in first row
    char strm[22] = "";

    #ifdef USECOLORS
    set_ucolor(win, &ucolors[MODE]);
    #endif

    if (curmode == NORMAL_MODE) {
        strcat(strm, " -- NORMAL --");
        write_j(win, strm, row, RIGHT);

    } else if (curmode == INSERT_MODE) {
        strcat(strm, " -- INSERT --");
        write_j(win, strm, row, RIGHT);

        #ifdef USECOLORS
        set_ucolor(win, &ucolors[INPUT]);
        #endif
        // Show submode (INSERT)
        mvwprintw(win, 0, 0 + rescol, "%c", insert_edit_submode);
        //wmove(win, 0, 1); commented on 01/06

    } else if (curmode == EDIT_MODE) {
        strcat(strm, "   -- EDIT --");
        write_j(win, strm, row, RIGHT);

    } else if (curmode == VISUAL_MODE) {
        strcat(strm, " -- VISUAL --");
        if (visual_submode != '0')
            strcpy(strm, " << VISUAL >>");
        write_j(win, strm, row, RIGHT);

    } else if (curmode == COMMAND_MODE) {
        strcat(strm, "-- COMMAND --");

        write_j(win, strm, row, RIGHT);
        #ifdef USECOLORS
        set_ucolor(win, &ucolors[INPUT]);
        #endif
        // muestro ':'
        mvwprintw(win, 0, 0 + rescol, ":");
        wmove(win, 0, 1 + rescol);
    }

    return;
}

// Show sc_row headings: 0, 1, 2, 3, 4...
void show_sc_row_headings(WINDOW * win, int mxrow) {
    int row = 0;
    #ifdef USECOLORS
    if (has_colors()) set_ucolor(win, &ucolors[HEADINGS]);
    #endif
    int i;
    for (i = offscr_sc_rows; i < mxrow; i++) {
        if (row_hidden[i]) continue;

        srange * s = get_selected_range();
        if ( (s != NULL && i >= s->tlrow && i <= s->brrow) || i == currow ) {
            #ifdef USECOLORS
            if (has_colors()) set_ucolor(win, &ucolors[CELL_SELECTION]);
            #else
            wattron(win, A_REVERSE);
            #endif
        }
        mvwprintw (win, row+1, 0, "%*d ", rescol-1, i);

        #ifdef USECOLORS
        if (has_colors()) set_ucolor(win, &ucolors[HEADINGS]);
        #else
        wattroff(win, A_REVERSE);
        #endif
        row++;
    }
}

// Show sc_col headings: A, B, C, D...
void show_sc_col_headings(WINDOW * win, int mxcol, int mxrow) {
    int i, col = rescol;

    #ifdef USECOLORS
    if (has_colors()) set_ucolor(win, &ucolors[HEADINGS]);
    #endif

    wmove(win, 0, 0);
    wclrtoeol(win);

    for (i = offscr_sc_cols; i <= mxcol; i++) {
        if (col_hidden[i]) continue;
        int k = fwidth[i] / 2;

        srange * s = get_selected_range();
        if ( (s != NULL && i >= s->tlcol && i <= s->brcol) || i == curcol ) {
            #ifdef USECOLORS
            if (has_colors()) set_ucolor(win, &ucolors[CELL_SELECTION]);
            #else
            wattron(win, A_REVERSE);
            #endif
        }
        (void) mvwprintw(win, 0, col, "%*s%-*s", k-1, " ", fwidth[i] - k + 1, coltoa(i));

        wclrtoeol(win);

        #ifdef USECOLORS
        if (has_colors()) set_ucolor(win, &ucolors[HEADINGS]);
        #else
        wattroff(win, A_REVERSE);
        #endif
        col += fwidth[i];
    }
}

// Show the content of the cells
void show_content(WINDOW * win, int mxrow, int mxcol) {

    register struct ent ** p;
    int row, col;
    int q_row_hidden = 0;

    for (row = offscr_sc_rows; row < mxrow; row++) {
        if (row_hidden[row]) {
            q_row_hidden++;
            continue;
        }

        register int c = rescol;
        int nextcol;
        int fieldlen;
        col = offscr_sc_cols;

        for (p = ATBL(tbl, row, offscr_sc_cols); col <= mxcol;
        p += nextcol - col, col = nextcol, c += fieldlen) {

            nextcol = col + 1;
            fieldlen = fwidth[col];
            if (col_hidden[col]) {
                c -= fieldlen;
                continue;
            }

            //if ( (*p) == NULL) *p = lookat(row, col);

            // Clean format
            #ifdef USECOLORS
            if ((*p) && (*p)->cellerror) {                                  // cellerror
                set_ucolor(win, &ucolors[CELL_ERROR]);
            } else if ((*p) && (*p)->expr) {
                set_ucolor(win, &ucolors[EXPRESSION]);
            } else if ((*p) && (*p)->label) {                               // string
                set_ucolor(win, &ucolors[STRG]);
            } else if ((*p) && (*p)->flags & is_valid && ! (*p)->format) {  // numeric value
                set_ucolor(win, &ucolors[NUMB]);
            } else if ((*p) && (*p)->format && (*p)->format[0] == 'd') {    // date format
                set_ucolor(win, &ucolors[DATEF]);
            } else {
                set_ucolor(win, &ucolors[NORMAL]);
            }
            #endif

            // Cell color!
            if ((*p) && (*p)->ucolor != NULL) {
                set_ucolor(win, (*p)->ucolor);
            }

            // Color selected cell
            if ((currow == row) && (curcol == col)) {
                #ifdef USECOLORS
                    if (has_colors()) set_ucolor(win, &ucolors[CELL_SELECTION_SC]);
                #else
                    wattron(win, A_REVERSE);
                #endif
            }

            // Color selected range
            int in_range = 0; // this is for coloring empty cells within a range
            srange * s = get_selected_range();
            if (s != NULL && row >= s->tlrow && row <= s->brrow && col >= s->tlcol && col <= s->brcol ) {
                #ifdef USECOLORS
                    set_ucolor(win, &ucolors[CELL_SELECTION_SC]);
                #else
                    wattron(win, A_REVERSE);
                #endif
                in_range = 1; // local variable. this is for coloring empty cells within a range
            }

            /* Color empty cells inside a range */
            if ( in_range && row >= ranges->tlrow && row <= ranges->brrow &&
                 col >= ranges->tlcol && col <= ranges->brcol
               ) {
                #ifdef USECOLORS
                    set_ucolor(win, &ucolors[CELL_SELECTION_SC]);
                #else
                    wattron(win, A_REVERSE);
                #endif
            }


            char num [FBUFLEN] = "";
            char text[FBUFLEN] = "";
            wchar_t out [FBUFLEN] = L"";
            char formated_s[FBUFLEN] = "";
            int res = -1;
            int align = 1;

            // If a numeric value exists
            if ( (*p) && (*p)->flags & is_valid) {
                //show_numeric_content_of_cell(win, p, col, row + 1 - offscr_sc_rows - q_row_hidden, c);

                res = get_formated_value(p, col, formated_s);
                // res = 0, indicates that in num we store a date
                // res = 1, indicates a format is applied in num
                if (res == 0 || res == 1) {
                    strcpy(num, formated_s);
                } else if (res == -1) {
                    sprintf(num, "%.*f", precision[col], (*p)->v);
                }
            }

            // If a string exists
            if ((*p) && (*p)->label) {
                strcpy(text, (*p)->label);
                align = 1;                               // right alignment
                if ((*p)->flags & is_label) {            // center alignment
                    align = 0;
                } else if ((*p)->flags & is_leftflush) { // left alignment
                    align = -1;
                } else if (res == 0) {                   // res must ¿NOT? be zero for label to be printed // TODO CHECK!
                    text[0] = '\0';
                }
            }

            if ((*p) && (*p)->cellerror == CELLERROR) {
               (void) mvprintw(row + RESROW + 1 - offscr_sc_rows, c, "%*.*s", fwidth[col], fwidth[col], "ERROR");
               align = 0;
               strcpy(text, "ERROR");
               num[0]='\0';
            }
            if ((*p) && (*p)->cellerror == CELLREF) {
               (void) mvprintw(row + RESROW + 1 - offscr_sc_rows, c, "%*.*s", fwidth[col], fwidth[col], "REF");
               align = 0;
               strcpy(text, "REF");
               num[0]='\0';
            }


            // repaint a blank cell, because of in range, or because we have a coloured empty cell!
            if ( !(*p) || (( !((*p)->flags & is_valid) && !(*p)->label ) && !((*p)->cellerror == CELLERROR)) ) {
                if ( (currow == row && curcol == col) ||
                ( in_range && row >= ranges->tlrow && row <= ranges->brrow &&
                col >= ranges->tlcol && col <= ranges->brcol ) ) {
                    #ifdef USECOLORS
                    if (has_colors()) set_ucolor(win, &ucolors[CELL_SELECTION_SC]);
                    #else
                    wattron(win, A_REVERSE);
                    #endif
                } else if ( !(*p) || (*p)->ucolor == NULL) {
                    #ifdef USECOLORS
                    set_ucolor(win, &ucolors[STRG]); // When a long string does not fit in column.
                    #endif
                }

                // new implementation for wide char support
                cchar_t cht[fieldlen];
                wchar_t w;
                int i, j;
                for (i = 0; i < fieldlen; ) {
                    w = L' ';
                    j = mvwin_wchnstr (win,  row + 1 - offscr_sc_rows - q_row_hidden, c + i, cht, 1);
                    if (j == OK && cht[0].chars[0] != L'\0')
                        w = cht[0].chars[0];
                    mvwprintw(win, row + 1 - offscr_sc_rows - q_row_hidden, c+i, "%lc", w);
                    i+= wcwidth(w);
                }

            // we print text and number
            } else {
                pad_and_align(text, num, fwidth[col], align, (*p)->pad, out);
                if (col == mxcol && wcswidth(out, wcslen(out)) > fwidth[col])
                    out[ count_width_widestring(out, fwidth[col]) ] = L'\0';

                mvwprintw(win, row + 1 - offscr_sc_rows - q_row_hidden, c, "%ls", out);
                wclrtoeol(win);
            }

            // clean format
            #ifndef USECOLORS
                wattroff(win, A_REVERSE);
            #endif
        }
    }
}

// Add details to ENT, used for 'input_win'
void add_cell_detail(char * d, struct ent * p1) {
    if ( ! p1 ) return;

    /* string expressions
    if (p1->expr && (p1->flags & is_strexpr)) {
        if (p1->flags & is_label)
            strcat(d, "|{");
        else
            strcat(d, (p1->flags & is_leftflush) ? "<{" : ">{");
        strcat(d, "??? } ");        // and this '}' is for vi %

    } else*/

    if (p1->label) {
        /* has constant label only */
        if (p1->flags & is_label)
            strcat(d, "|\"");
        else
            strcat(d, (p1->flags & is_leftflush) ? "<\"" : ">\"");
        strcat(d, p1->label);
        strcat(d, "\" ");
    }

    /* Display if cell is locked */
    if (p1 && p1->flags & is_locked)
         strcat(d, "[locked] ");

    // value part of cell:
    if (p1->flags & is_valid) {
        /* has value or num expr */
        if ( ( ! (p1->expr) ) || ( p1->flags & is_strexpr ) ) {
            sprintf(d + strlen(d), "%c", '[');
            (void) sprintf(d + strlen(d), "%.15g", p1->v);
            sprintf(d + strlen(d), "%c", ']');
        }
    }
}

// Draw cell content detail in header
void show_celldetails(WINDOW * win) {
    char head[FBUFLEN];
    int inputline_pos = 0;

    // show cell in header
    #ifdef USECOLORS
        set_ucolor(win, &ucolors[CELL_ID]);
    #endif
    sprintf(head, "%s%d ", coltoa(curcol), currow);
    mvwprintw(win, 0, 0 + rescol, "%s", head);
    inputline_pos += strlen(head) + rescol;

    // show the current cell's format
    #ifdef USECOLORS
        set_ucolor(win, &ucolors[CELL_FORMAT]);
    #endif

    register struct ent *p1 = *ATBL(tbl, currow, curcol);

    // show padding
    if (p1 != NULL && p1->pad)
        sprintf(head, "(%d) ", p1->pad);
    else
        head[0]='\0';

    // show format
    if ((p1) && p1->format)
        sprintf(head + strlen(head), "(%s) ", p1->format);
    else
        sprintf(head + strlen(head), "(%d %d %d) ", fwidth[curcol], precision[curcol], realfmt[curcol]);
    mvwprintw(win, 0, inputline_pos, "%s", head);
    inputline_pos += strlen(head);

    // show expr
    #ifdef USECOLORS
        set_ucolor(win, &ucolors[CELL_CONTENT]);
    #endif
    if (p1 && p1->expr) {
        linelim = 0;
        editexp(currow, curcol);  /* set line to expr */
        linelim = -1;
        sprintf(head, "[%s] ", line);
        mvwprintw(win, 0, inputline_pos, "%s", head);
        inputline_pos += strlen(head);
    }
    // add cell content to head string 
    head[0] = '\0';
    add_cell_detail(head, p1);

    // cut string if its too large!
    if (strlen(head) > COLS - inputline_pos - 1) {
        head[COLS - inputline_pos - 1 - 15]='>';
        head[COLS - inputline_pos - 1 - 14]='>';
        head[COLS - inputline_pos - 1 - 13]='>';
        head[COLS - inputline_pos - 1 - 12]='\0';
    }

    mvwprintw(win, 0, inputline_pos, "%s", head);

    wclrtoeol(win);
}

// Calculate number of hide rows above
int calc_offscr_sc_rows() {
    // pick up row counts
    int i, rows = 0, row = 0;
    if (offscr_sc_rows <= currow)
        for (i = offscr_sc_rows, rows = 0, row=RESROW; i < maxrows && row < LINES; i++) { //i <= maxrows
            rows++;
            if (i == maxrows - 1) return rows+1;
            if (! row_hidden[i])
                row++;
        }
    // get off screen rows
    while ( offscr_sc_rows + rows - 2 < currow || currow < offscr_sc_rows ) {
        if (offscr_sc_rows - 1 == currow) offscr_sc_rows--;
        else if (offscr_sc_rows + rows - 1 == currow) offscr_sc_rows++;
        else  {
            // Try to put the cursor in the center of the screen
            row = (LINES - RESROW) / 2 + RESROW;
            //row = (LINES - RESROW) / 2;
            offscr_sc_rows = currow;
            for (i=currow-1; i >= 0 && row - 1 > RESROW && i < maxrows; i--) {
            //for (i=currow-1; i >= 0 && row - 1 > 0 && i < maxrows; i--) {
                offscr_sc_rows--;
                if (! row_hidden[i])
                    row--;
            }
        }
        // Now pick up the counts again
        for (i = offscr_sc_rows, rows = 0, row = RESROW; i < maxrows && row < LINES; i++) { //i <= maxrows
        //for (i = offscr_sc_rows, rows = 0, row = 0; i < maxrows && row < LINES; i++) { //i <= maxrows
            rows++;
            if (i == maxrows - 1) return rows+1;
            if (! row_hidden[i])
                row++;
        }
    }
    return rows;
}

// Calculate number of hidden columns in the left
int calc_offscr_sc_cols() {
    int i, cols = 0, col = 0;
    // pick up col counts
    if (offscr_sc_cols <= curcol)
        for (i = offscr_sc_cols, cols = 0, col = rescol; i < maxcols && col + fwidth[i] - 1 < COLS - 1; i++) {
            cols++;
            if (! col_hidden[i])
                col += fwidth[i];
        }
    // get off screen cols
    while ( offscr_sc_cols + cols - 1 < curcol || curcol < offscr_sc_cols ) {
        if (offscr_sc_cols - 1 == curcol) offscr_sc_cols--;
        else if (offscr_sc_cols + cols == curcol) offscr_sc_cols++;
        else {
            // Try to put the cursor in the center of the screen
            col = (COLS - rescol - fwidth[curcol]) / 2 + rescol;
            offscr_sc_cols = curcol;
            for (i=curcol-1; i >= 0 && col-fwidth[i] - 1 > rescol; i--) {
                offscr_sc_cols--;
                if (! col_hidden[i])
                    col -= fwidth[i];
            }
        }

        // Now pick up the counts again
        for (i = offscr_sc_cols, cols = 0, col = rescol; i < maxcols && col + fwidth[i] - 1  < COLS - 1; i++) {
            cols++;
            if (! col_hidden[i])
                col += fwidth[i];
        }
    }
    return cols;
}

// error routine for yacc (gram.y)
void yyerror(char * err) {
    mvwprintw(input_win, 1, 0, "%s: %.*s<=%s", err, linelim, line, line + linelim);
    wrefresh(input_win);
    return;
}

// this function creates a string (value) that represents the formated value of the cell, if a format exists
// returns 0  datetime format - number in p->v represents a date - format "d"
// returns 1  format of number - (numbers with format) - puede haber label.
// returns -1 if there is no format in the cell.
int get_formated_value(struct ent ** p, int col, char * value) {
    //char * cfmt = (*p)->format ? (*p)->format : NULL;
    char * cfmt = (*p)->format ? (*p)->format : (realfmt[col] >= 0 && realfmt[col] < COLFORMATS && colformat[realfmt[col]] != NULL) ? colformat[realfmt[col]] : NULL;

    if (cfmt) {
        if (*cfmt == 'd') {
            time_t v = (time_t) ((*p)->v);
            strftime(value, sizeof(char) * FBUFLEN, cfmt + 1, localtime(&v));
            return 0;
        } else {
            format(cfmt, precision[col], (*p)->v, value, sizeof(char) * FBUFLEN);
            return 1;
        }
    } else { // there is no format
        return -1;
    }
}

// this function aligns text of a cell (align = 0 center, align = 1 right, align = -1 left)
// and adds padding between cells.
// returns resulting string to be printed in screen.
void pad_and_align (char * str_value, char * numeric_value, int col_width, int align, int padding, wchar_t * str_out) {
    int str_len  = 0;
    int num_len  = strlen(numeric_value);
    str_out[0] = L'\0';

    wchar_t wcs_value[BUFFERSIZE] = { L'\0' };
    mbstate_t state;
    size_t result;
    const char * mbsptr;
    mbsptr = str_value;


    // create wcs string based on multibyte string..
    memset( &state, '\0', sizeof state );
    result = mbsrtowcs(wcs_value, &mbsptr, BUFFERSIZE, &state);
    if ( result != (size_t)-1 )
        str_len = wcswidth(wcs_value, wcslen(wcs_value));

    // If padding exceedes column width, returns n number of '-' needed to fill column width
    if (padding >= col_width ) {
        wmemset(str_out + wcslen(str_out), L'#', col_width);
        return;
    }

    // If content exceedes column width, outputs n number of '*' needed to fill column width
    if (str_len + num_len + padding > col_width && ( (! atoi(get_conf_value("overlap"))) || align == 1) ) {
        if (padding) wmemset(str_out + wcslen(str_out), L'#', padding);
        wmemset(str_out + wcslen(str_out), L'*', col_width - padding);
        return;
    }

    // padding
    if (padding) swprintf(str_out, BUFFERSIZE, L"%*ls", padding, L"");

    // left spaces
    int left_spaces = 0;
    if (align == 0 && str_len) {                           // center align
        left_spaces = (col_width - padding - str_len) / 2;
        if (num_len > left_spaces) left_spaces = col_width - padding - str_len - num_len;
    } else if (align == 1 && str_len && ! num_len) {       // right align
        left_spaces = col_width - padding - str_len;
    }
    while (left_spaces-- > 0) add_wchar(str_out, L' ', wcslen(str_out));

    // add text
    if (align != 1 || ! num_len)
        swprintf(str_out + wcslen(str_out), BUFFERSIZE, L"%s", str_value);

    // spaces after string value
    int spaces = col_width - padding - str_len - num_len;
    if (align == 1) spaces += str_len;
    if (align == 0) spaces -= (col_width - padding - str_len) / 2;
    while (spaces-- > 0) add_wchar(str_out, L' ', wcslen(str_out));

    // add number
    int fill_with_number = col_width - str_len - padding;
    if (num_len && num_len >= fill_with_number) {
        swprintf(str_out + wcslen(str_out), BUFFERSIZE, L"%.*s", fill_with_number, & numeric_value[num_len - fill_with_number]);
    } else if (num_len) {
        swprintf(str_out + wcslen(str_out), BUFFERSIZE, L"%s", numeric_value);
    }

    return;
}

// function that shows text in a child process
void show_text(char * val) {
    int pid;
    char px[MAXCMD];
    char * pager;

    (void) strcpy(px, "| ");
    if ( !(pager = getenv("PAGER")) )
        pager = DFLT_PAGER;
    (void) strcat(px, pager);
    FILE * f = openfile(px, &pid, NULL);
    if ( !f ) {
        sc_error("Can't open pipe to %s", pager);
        return;
    }
    def_prog_mode();
    endwin();
    fprintf(f, "%s\n", val);
    fprintf(f, "Press 'q' and then ENTER to return.");
    closefile(f, pid, 0);
    getchar();
    reset_prog_mode();
    refresh();
    update(TRUE);
}
