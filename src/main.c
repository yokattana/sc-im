#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>   // for F_GETFL O_NONBLOCK F_SETFL
#include <locale.h>
#include <wchar.h>
#include <wordexp.h>
#include <sys/ioctl.h> // for ioctl

#include "main.h"
#include "shift.h"
#include "macros.h"
#include "tui.h"
#include "input.h"
#include "marks.h"
#include "maps.h"
#include "yank.h"
#include "file.h"
#include "utils/dictionary.h"
#include "utils/string.h"
#include "history.h"
#include "conf.h"
#include "buffer.h"
#include "cmds.h"
#include "vmtbl.h"   // for growtbl
#include "filter.h"
#include "dep_graph.h"

#ifdef UNDO
#include "undo.h"
#endif

#ifdef XLUA
#include "lua.h"
#endif

int currow = 0, curcol = 0;
int lastrow = 0, lastcol = 0;
int maxrows, maxcols;
int * fwidth;
int * precision;
int * realfmt;
char * col_hidden;
char * row_hidden;
char line[FBUFLEN];
int modflg;          // a change was made since last save
struct ent *** tbl;
int shall_quit = 0;
unsigned int curmode;
unsigned int lastmode;
int maxrow, maxcol;
char curfile[PATHLEN];
char loadingfile[PATHLEN] = { '\0' };
char * exepath;

int changed;
int cellassign;
int arg = 1;
int brokenpipe = FALSE; /* Set to true if SIGPIPE is received */
char * ascext;
char * tbl0ext;
char * tblext;
char * latexext;
char * slatexext;
char * texext;
char dpoint = '.';      // decimal point
char thsep = ',';       // thousands separator
int linelim = -1;
int calc_order = BYROWS;
int optimize  = 0;      // Causes numeric expressions to be optimized
int tbl_style = 0;      // headers for T command output
int rndtoeven = 0;
int rowsinrange = 1;
int colsinrange = DEFWIDTH;
double eval_result;
char * seval_result;
FILE * fdoutput;        // output file descriptor (stdout or file)
int rescol = RESCOL;    // columns reserved for row numbers

struct block * buffer;
struct block * lastcmd_buffer;
struct dictionary * user_conf_d;
struct dictionary * predefined_conf_d;
struct history * commandline_history;
struct history * insert_history;
char stderr_buffer[1024] = "";
struct timeval startup_tv, current_tv; //runtime timer

#ifdef AUTOBACKUP
struct timeval lastbackup_tv; // last backup timer
#ifdef HAVE_PTHREAD
#include <pthread.h>
int pthread_exists = 0;       // return status of pthread_create
pthread_t fthread;
#endif
#endif

void read_stdin();
extern char * rev;

/*********************************************************************
 * MAIN LOOP
 *********************************************************************/
int main (int argc, char ** argv) {
    if (setvbuf(stderr, stderr_buffer, _IOFBF, STDERRBUF) != 0) {
        fprintf(stderr, "Error setting stderr buffer\n");
        return -1;
    }

    // set up signals so we can catch them
    signals();

#ifdef USELOCALE
    // pass LC_CTYPE env variable to libraries
    //setlocale(LC_ALL, "");
    setlocale(LC_CTYPE, "");
#endif

    // start configuration dictionaries
    user_conf_d = (struct dictionary *) create_dictionary();
    predefined_conf_d = (struct dictionary *) create_dictionary();
    store_default_config_values();

    // we save parameters and use them to replace conf-values in config dictionary !
    read_argv(argc, argv);

    // check if version is in argv. if so, show version and quit
    if (atoi(get_conf_value("version")))
        show_version_and_quit();

    // create command line history structure
    if (! atoi(get_conf_value("nocurses"))) {
#ifdef HISTORY_FILE
        commandline_history = (struct history *) create_history(':');
        load_history(commandline_history, ':');
#endif
#ifdef INS_HISTORY_FILE
        insert_history = (struct history *) create_history('=');
        load_history(insert_history, '=');
#endif
    }

    // create basic structures that will depend on the loaded file
    create_structures();

    // setup the spreadsheet arrays (tbl)
    if (! growtbl(GROWNEW, 0, 0)) return exit_app(1);


    // initiate NCURSES if that is what is wanted
    if (! atoi(get_conf_value("nocurses")))
        ui_start_screen();

#ifdef USECOLORS
    if (get_d_colors_param() == NULL) {
        start_default_ucolors();
        /*
         * in case we decide to change colors
         * this creates a dictionary and stores in it
         * the relationship between macros and the keys values
         * that are defined in .sc files
         */
        set_colors_param_dict();
    }
#endif

    /*
     * If the 'output' parameter is defined, SC-IM saves its output to that file.
     * To achieve that, we open the output file and keep it open until exit.
     * otherwise, SC-IM will output to stdout.
     */
    if (get_conf_value("output") != NULL) {
        fdoutput = fopen(get_conf_value("output"), "w+");
        if (fdoutput == NULL) {
            sc_error("Cannot open file: %s.", get_conf_value("output"));
            return exit_app(-1);
        }

        if (! atoi(get_conf_value("nocurses"))) { // WE MUST STOP SCREEN!
            ui_stop_screen();

            // if output is set, nocurses should always be 1 !
            put(user_conf_d, "nocurses", "1");
        }
    }

#ifdef XLUA
    doLuainit();
#endif

    wchar_t stdin_buffer[BUFFERSIZE] = { L'\0' };

    // if there was no file passed to scim executable
    // 1. erase db !
    if (! loadingfile[0]) erasedb();

    // 2. loadrc
    loadrc();

    // 3. check input from stdin (pipeline)
    // and send it to interp
    read_stdin();

    // 4. read sc file passed as argv
    load_sc();

    // change curmode to NORMAL_MODE
    chg_mode('.');

    // initiate ui
    FILE * f;
    if ( ! atoi(get_conf_value("nocurses"))) {
        // we show welcome screen if no spreadsheet was passed to SC-IM
        // and no input was sent throw pipeline
        if ( ! curfile[0] && ! wcslen(stdin_buffer)) {
            ui_do_welcome();
            // show mode and cell's details in status bar
            ui_print_mode();
            ui_show_celldetails();
        } else {
            ui_show_header();
            ui_update(TRUE);
        }
    } else {
        f = fopen("/dev/tty", "rw");
        if (f == NULL) sc_error("fatal error loading stdin");
    }

    // handle input from keyboard
    if (! atoi(get_conf_value("nocurses")))
        buffer = (struct block *) create_buf(); // this should only take place if curses ui

    wchar_t nocurses_buffer[BUFFERSIZE];

    // runtime timer
    gettimeofday(&startup_tv, NULL);

    #ifdef AUTOBACKUP
    //gettimeofday(&lastbackup_tv, NULL);
    lastbackup_tv = (struct timeval) {0};
    #endif

    while ( ! shall_quit && ! atoi(get_conf_value("quit_afterload"))) {
        // save current time for runtime timer
        gettimeofday(&current_tv, NULL);

        // autobackup if it is time to do so
        handle_backup();

        // if we are in ncurses
        if (! atoi(get_conf_value("nocurses"))) {
            handle_input(buffer);

        // if we are not in ncurses
        } else if (fgetws(nocurses_buffer, BUFFERSIZE, f) != NULL) {
            sc_info("Interp will receive: %ls", nocurses_buffer);
            send_to_interp(nocurses_buffer);
        }

        /* shall_quit=1 means :q
           shall_quit=2 means :q! */
        if (shall_quit == 1 && modcheck()) shall_quit = 0;
    }
    if (atoi(get_conf_value("nocurses")) && f != NULL) fclose(f);

    return shall_quit == -1 ? exit_app(-1) : exit_app(0);
}
/*********************************************************************
 * END OF MAIN LOOP
 *********************************************************************/

extern graphADT graph;

void create_structures() {

    // initiate mark array
    create_mark_array();

    // create last command buffer
    lastcmd_buffer = (struct block *) create_buf();

    // create yank list structure
    init_yanklist();

    /* Assign NULL to colformats
    int c;
    for (c = 0; c < COLFORMATS; c++)
        colformat[c] = NULL;
    */

    // init calc chain graph
    graph = GraphCreate();
}

void read_stdin() {
    //sc_debug("reading stdin from pipeline");
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    fd_set savefds = readfds;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    FILE * f = stdin;
    //FILE * f = fopen("/dev/tty", "rw");
    wchar_t stdin_buffer[BUFFERSIZE] = { L'\0' };

    if (select(1, &readfds, NULL, NULL, &timeout)) {
        //sc_debug("there is data");
        while (f != NULL && fgetws(stdin_buffer, BUFFERSIZE, f) != NULL) {
            sc_info("Interp will receive: %ls", stdin_buffer);
            send_to_interp(stdin_buffer);
        }
        fflush(f);
    } else {
        //sc_debug("there is NO data");
    }
    readfds = savefds;
    if (f != NULL) fclose(f);

    if ( ! freopen("/dev/tty", "rw", stdin)) {
        perror(NULL);
        exit(-1);
    }
    //sc_debug("finish reading");
}

// delete basic structures that depend on the loaded file
void delete_structures() {

    // Free marks array
    free_marks_array();

    // Free yanklist
    free_yanklist();

    // Erase last_command buffer
    erase_buf(lastcmd_buffer);

    // Free ranges
    free_ranges();

    // Free filters
    free_filters();

    // Free undo list - from start of list
#ifdef UNDO
    clear_undo_list();
#endif

    // free deleted ents
    flush_saved();

    // free calc chain graph
    destroy_graph(graph);

    // Free ents of tbl
    erasedb();
}

int exit_app(int status) {

    // free history
    if (! atoi(get_conf_value("nocurses"))) {

#ifdef HISTORY_FILE
        if (! save_history(commandline_history, "w")) sc_error("Could not save commandline history");
        if (commandline_history != NULL) destroy_history(commandline_history);
#endif

#ifdef INS_HISTORY_FILE
        if (! save_history(insert_history, "a")) sc_error("Could not save input mode history");
        if (insert_history != NULL) destroy_history(insert_history);
#endif
    }

    // wait for autobackup thread to finish, just in case
    #if defined(AUTOBACKUP) && defined(HAVE_PTHREAD)
    if (pthread_exists) pthread_join (fthread, NULL);
    #endif

    // remove backup file
#ifdef AUTOBACKUP
    if (strlen(curfile) && backup_exists(curfile)) remove_backup(curfile);
#endif

    // erase structures
    delete_structures();

    // Free mappings
    del_maps();

    // Erase stdin
    erase_buf(buffer);

    // stop CURSES screen
    if (! atoi(get_conf_value("nocurses")))
        ui_stop_screen();

    // close fdoutput
    if (get_conf_value("output") != '\0' && fdoutput != NULL) {
        fclose(fdoutput);
    }

    // delete user and predefined config dictionaries
    destroy_dictionary(predefined_conf_d);
    destroy_dictionary(user_conf_d);

    return status;
}

/*
 * we read parameters passed to SC-IM executable
 * and store them in user_conf dictionary
 */
void read_argv(int argc, char ** argv) {
    int i;
    for (i = 1; i < argc; i++) {
        if ( ! strncmp(argv[i], "--", 2) ) {       // it was passed a parameter
            char *dup = strdup(argv[i]);
            char *rest = dup;
            char *name = strsep(&rest, "=");
            if (rest) {
                put(user_conf_d, &name[2], rest);  // --parameter=value
            } else {
                put(user_conf_d, &name[2], "1");   // --parameter
            }
            free(dup);
        } else {                                   // it was passed a file
            strncpy(loadingfile, argv[i], PATHLEN-1);
        }
    }
    exepath = argv[0];
    return;
}

// we try to load a file
void load_sc() {
    wordexp_t p;
    wordexp(loadingfile, &p, 0);

    int c;
    char word[PATHLEN] = "";
    for (c=0; c < p.we_wordc; c++) {
        if (c) sprintf(word + strlen(word), " ");
        sprintf(word + strlen(word), "%s", p.we_wordv[c]);
    }
    if (strlen(word) && ! readfile(word, 0) && ! atoi(get_conf_value("nocurses"))) {
        sc_info("New file: \"%s\"", word);     // file passed to scim executable does not exists
    }
    wordfree(&p);
    return;
}

// set the calculation order
void setorder(int i) {
    if ((i == BYROWS) || (i == BYCOLS)) calc_order = i;
    return;
}

void nopipe() {
    sc_error("brokenpipe!");
    brokenpipe = TRUE;
    return;
}

// setup signals catched by SC-IM
void signals() {
    void sig_int();
    void sig_abrt();
    void sig_term();
    void nopipe();
    void winchg();

    signal(SIGINT, sig_int);
    signal(SIGABRT, sig_abrt);
    signal(SIGTERM, sig_term); // kill
    signal(SIGPIPE, nopipe);
    //(void) signal(SIGALRM, time_out);
    signal(SIGWINCH, winchg);
    //(void) signal(SIGBUS, doquit);
    //(void) signal(SIGFPE, doquit);
    return;
}

void sig_int() {
    if ( ! atoi(get_conf_value("debug")))
        sc_error("Got SIGINT. Press «:q<Enter>» to quit SC-IM");
    else
        shall_quit = 2;
    return;
}

void sig_abrt() {
    sc_error("Error !!! Quitting SC-IM.");
    shall_quit = -1; // error !
    return;
}

void sig_term() {
    sc_error("Got SIGTERM signal. Quitting SC-IM.");
    shall_quit = 2;
    return;
}

void show_version_and_quit() {
    put(user_conf_d, "nocurses", "1");
    sc_info("Sc-im - %s", rev);
#ifdef NCURSES
    sc_info("-DNCURSES");
#endif
#ifdef MAXROWS
    sc_info("-DMAXROWS %d", MAXROWS);
#endif
#ifdef UNDO
    sc_info("-DUNDO");
#endif
#ifdef XLS
    sc_info("-DXLS");
#endif
#ifdef XLSX
    sc_info("-DXLSX");
#endif
#ifdef XLSX_EXPORT
    sc_info("-DXLSX_EXPORT");
#endif
#ifdef XLUA
    sc_info("-DXLUA");
#endif
#ifdef DEFAULT_COPY_TO_CLIPBOARD_CMD
    sc_info("-DDEFAULT_COPY_TO_CLIPBOARD_CMD=\"%s\"", DEFAULT_COPY_TO_CLIPBOARD_CMD);
#endif
#ifdef DEFAULT_PASTE_FROM_CLIPBOARD_CMD
    sc_info("-DDEFAULT_PASTE_FROM_CLIPBOARD_CMD=\"%s\"", DEFAULT_PASTE_FROM_CLIPBOARD_CMD);
#endif
#ifdef USELOCALE
    sc_info("-DUSELOCALE");
#endif
#ifdef USECOLORS
    sc_info("-DUSECOLORS");
#endif
#ifdef _XOPEN_SOURCE_EXTENDED
    sc_info("-D_XOPEN_SOURCE_EXTENDED");
#endif
#ifdef _GNU_SOURCE
    sc_info("-D_GNU_SOURCE");
#endif
#ifdef SNAME
    sc_info("-DSNAME=\"%s\"", SNAME);
#endif
#ifdef HELP_PATH
    sc_info("-DHELP_PATH=\"%s\"", HELP_PATH);
#endif
#ifdef LIBDIR
    sc_info("-DLIBDIR=\"%s\"", LIBDIR);
#endif
#ifdef DFLT_PAGER
    sc_info("-DDFLT_PAGER=\"%s\"", DFLT_PAGER);
#endif
#ifdef HISTORY_FILE
    sc_info("-DHISTORY_FILE=\"%s\"", HISTORY_FILE);
#endif
#ifdef INS_HISTORY_FILE
    sc_info("-DINS_HISTORY_FILE=\"%s\"", INS_HISTORY_FILE);
#endif
#ifdef HAVE_PTHREAD
    sc_info("-DHAVE_PTHREAD");
#endif
#ifdef AUTOBACKUP
    sc_info("-DAUTOBACKUP");
#endif
    put(user_conf_d, "quit_afterload", "1");
}
