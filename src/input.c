#include <sys/time.h>
#include <string.h>
#include <ctype.h>   // for isdigit
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

#include "main.h"
#include "tui.h"
#include "maps.h"
#include "cmds.h"
#include "history.h"
#include "conf.h"
#include "utils/string.h"
#include "cmds_visual.h"
#include "buffer.h"

static wint_t wd;          // char read from stdin
static int d;              // char read from stdin
int return_value;          // return value of getch()
int cmd_multiplier = 0;    // Multiplier
int cmd_pending = 0;       // Command pending
int shall_quit;            // Break loop if ESC key is pressed

/*
 * Reads stdin for a valid command.
 * Details: Read characters from stdin to a input buffer.
 * When filled up, validate the command and call the appropriate handler.
 * When a timeout is reached, flush the buffer.
 */
void handle_input(struct block * buffer) {
    struct timeval start_tv, m_tv; // For measuring timeout
    gettimeofday(&start_tv, NULL);
    gettimeofday(&m_tv, NULL);
    long msec = (m_tv.tv_sec - start_tv.tv_sec) * 1000L +
                (m_tv.tv_usec - start_tv.tv_usec) / 1000L;

    cmd_multiplier = 0;
    cmd_pending = 0;

    while ( ! has_cmd(buffer, msec) && msec <= CMDTIMEOUT ) {
            // if command pending, refresh 'ef' only. Multiplier and cmd pending
            if (cmd_pending) ui_print_mult_pend();

            // Modify cursor state according to the current mode
            ui_handle_cursor();

            // Read new character from stdin
            return_value = ui_getch(&wd);
            d = wd;
            if ( d == OKEY_ESC) {
                break_waitcmd_loop(buffer);
                return;
            }

            // Handle multiplier of commands in NORMAL mode.
            if ( return_value != -1 && isdigit(d)
               && ( buffer->value == L'\0' || iswdigit((wchar_t) buffer->value))
               && ( curmode == NORMAL_MODE || curmode == VISUAL_MODE || curmode == EDIT_MODE )
               && ( cmd_multiplier || d != L'0' )
               && ( ! atoi(get_conf_value("numeric")))
               ) {
                    cmd_multiplier *= 10;
                    cmd_multiplier += (int) (d - '0');
                    if (cmd_multiplier > MAX_MULTIPLIER) cmd_multiplier = 0;

                    gettimeofday(&start_tv, NULL);
                    msec = (m_tv.tv_sec - start_tv.tv_sec) * 1000L +
                           (m_tv.tv_usec - start_tv.tv_usec) / 1000L;

                    ui_print_mult_pend();
                    continue;
            }

            /*
             * Update time stap to reset timeout after each loop
             * (Only if current mode is COMMAND, INSERT or EDIT) and for each
             * user input as well.
             */
            fix_timeout(&start_tv);

            /*
             * Handle special characters input: BS TAG ENTER HOME END DEL PGUP
             * PGDOWN and alphanumeric characters
             */
            if (is_idchar(d) || return_value != -1) {
                // If in NORMAL, VISUAL or EDITION mode , change top left corner indicator
                if ( (curmode == NORMAL_MODE && d >= ' ') || //FIXME
                     (curmode == EDIT_MODE   && d >= ' ') ||
                     (curmode == VISUAL_MODE && d >= ' ') ) {
                    cmd_pending = 1;
                }
                addto_buf(buffer, wd);

                // Replace maps in buffer
                replace_maps(buffer);
            }

            gettimeofday(&m_tv, NULL);
            msec = (m_tv.tv_sec - start_tv.tv_sec) * 1000L +
                   (m_tv.tv_usec - start_tv.tv_usec) / 1000L;
    }
    if (msec >= CMDTIMEOUT) { // timeout. Command incomplete
        cmd_pending = 0;      // No longer wait for a command, set flag.
        cmd_multiplier = 0;   // Reset multiplier
    } else {                  // Execute command or mapping
        cmd_pending = 0;
        ui_clr_header(1);     // Clean second line
        handle_mult( &cmd_multiplier, buffer, msec ); // Handle command and repeat as many times as the multiplier dictates
    }
    ui_print_mult_pend();
    flush_buf(buffer);        // Flush the buffer
    return;
}

// Break waiting command loop
void break_waitcmd_loop(struct block * buffer) {
    if (curmode == COMMAND_MODE) {
#ifdef HISTORY_FILE
        del_item_from_history(commandline_history, 0);
        commandline_history->pos = 0;
        set_comp(0);
#endif
    } else if (curmode == INSERT_MODE) {
#ifdef INS_HISTORY_FILE
        del_item_from_history(insert_history, 0);
        insert_history->pos = 0;
#endif
    } else if (curmode == VISUAL_MODE) {
        exit_visualmode();
    }
    if (curmode == INSERT_MODE && lastmode == EDIT_MODE)     {
        if (inputline_pos && wcslen(inputline) >= inputline_pos) {
            real_inputline_pos--;
            int l = wcwidth(inputline[real_inputline_pos]);
            inputline_pos -= l;
        }
        chg_mode(insert_edit_submode == '=' ? 'e' : 'E');
        lastmode=NORMAL_MODE;
        ui_show_header();
    } else if (curmode == EDIT_MODE && lastmode == INSERT_MODE) {
        chg_mode(insert_edit_submode);
        lastmode=NORMAL_MODE;
        ui_show_header();
    } else {
        chg_mode('.');
        lastmode=NORMAL_MODE;
        inputline[0] = L'\0';  // clean inputline
        flush_buf(buffer);
        ui_update(TRUE);
    }
    cmd_pending = 0;       // No longer wait for command. Set flag.
    cmd_multiplier = 0;    // Reset the multiplier
    return;
}

/*
 * Handle timeout depending on the current mode
 * there is NO timeout for COMMAND, INSERT and EDIT modes.
 */
void fix_timeout(struct timeval * start_tv) {
    switch (curmode) {
        case COMMAND_MODE:
        case INSERT_MODE:
            gettimeofday(start_tv, NULL);
            break;
        case VISUAL_MODE:
        case EDIT_MODE:
        case NORMAL_MODE:
            if (d != 0) gettimeofday(start_tv, NULL);
    }
    return;
}

/*
 * Traverse 'stuffbuff' and determines if there  is a valid command
 * Ej. buffer = "diw"
 */
int has_cmd (struct block * buf, long timeout) {
    int len = get_bufsize(buf);
    if ( ! len ) return 0;
    int k, found = 0;

    struct block * auxb = (struct block *) create_buf();

    for (k = 0; k < len; k++) {
        addto_buf(auxb, get_bufval(buf, k));
        if ( is_single_command(auxb, timeout)) { found = 1; break; }
    }
    erase_buf(auxb);
    auxb = NULL;
    return found;
}

void do_commandmode(struct block * sb);
void do_normalmode (struct block * buf);
void do_insertmode(struct block * sb);
void do_editmode(struct block * sb);
void do_visualmode(struct block * sb);

// Use specific functions for every command on each mode
void exec_single_cmd (struct block * sb) {
    switch (curmode) {
        case NORMAL_MODE:
            do_normalmode(sb);
            break;
        case INSERT_MODE:
            do_insertmode(sb);
            break;
        case COMMAND_MODE:
            do_commandmode(sb);
            break;
        case EDIT_MODE:
            do_editmode(sb);
            break;
        case VISUAL_MODE:
            do_visualmode(sb);
            break;
    }
    return;
}

// Handle the final command to be executed, using the multiplier
void handle_mult(int * cmd_multiplier, struct block * buf, long timeout) {
    int j, k;
    struct block * b_copy = buf;
    int lenbuf = get_bufsize(b_copy);
    if ( ! *cmd_multiplier) *cmd_multiplier = 1;

    for (j = 1; j < *cmd_multiplier; j++) {
        for (k = 0; k < lenbuf; k++) {
            addto_buf(buf, b_copy->value);
            b_copy = b_copy->pnext;
        }
    }
    //if (is_single_command(buf, timeout) == EDITION_CMD)
    //    copybuffer(buf, lastcmd_buffer); // save stdin buffer content in lastcmd buffer
    exec_mult(buf, timeout);
    if (*cmd_multiplier > 1) { *cmd_multiplier = 1; ui_update(TRUE); }
    *cmd_multiplier = 0;

    return;
}

// Handle multiple command execution in sequence
void exec_mult (struct block * buf, long timeout) {
    int k, res, len = get_bufsize(buf);
    if ( ! len ) return;

    // Try to execute the whole buffer content
    if ((res = is_single_command(buf, timeout))) {
        if (res == EDITION_CMD) copybuffer(buf, lastcmd_buffer); // save stdin buffer content in lastcmd buffer
        exec_single_cmd(buf);

    // If not possible, traverse blockwise
    } else {
        struct block * auxb = (struct block *) create_buf();
        for (k = 0; k < len; k++) {
            addto_buf(auxb, get_bufval(buf, k));

            if ((res = is_single_command(auxb, timeout))) {
                if (res == EDITION_CMD) copybuffer(buf, lastcmd_buffer); // save stdin buffer content in lastcmd buffer
                exec_single_cmd(auxb);
                flush_buf(auxb);
                k++; // Take the first K values from 'buf'
                while ( k-- ) buf = dequeue(buf);
                // Execute again
                if (cmd_multiplier == 0) break;
                exec_mult (buf, timeout);
                break;
            }
        }
        erase_buf(auxb);
        auxb = NULL;
    }
    return;
}
