int main(int argc, char ** argv);
int exit_app(int status);
void create_structures();
void delete_structures();
void load_sc();
void read_argv(int argc, char ** argv);
void setorder(int i);
void nopipe();
void signals();
void show_version_and_quit();

// SIGINT signal
void sig_int();

// SIGWINCH signal - resize of terminal
void winchg();

extern FILE * fdoutput; // output file descriptor (stdout or file)
extern unsigned int curmode;
extern unsigned int lastmode;
extern struct timeval startup_tv, current_tv; //runtime timer
