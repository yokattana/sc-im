struct undo {
    struct undo * p_ant;
    struct ent * added;
    struct ent * removed;
    struct ent * aux_ents;          // add e_new to beginning of list
    struct undo_range_shift * range_shift;
    struct undo_cols_format * cols_format;
    struct undo * p_sig;
    int * row_hidded;
    int * row_showed;
    int * col_hidded;
    int * col_showed;
};

struct undo_range_shift {
    int delta_rows;
    int delta_cols;
    int tlrow;
    int tlcol;
    int brrow;
    int brcol;
};

//These two structures are for undo / redo changes in column format
struct undo_col_info {
    char type;       // a column can be 'R' (removed) or 'A' (added) because of change
    int col;
    int fwidth;
    int precision;
    int realfmt;
};

struct undo_cols_format {
    size_t length;   // keep the number of elements (cols)
    struct undo_col_info * cols;
};

void create_undo_action();
void end_undo_action();
void copy_to_undostruct (int row_desde, int col_desde, int row_hasta, int col_hasta, char type);
void save_undo_range_shift(int delta_rows, int delta_cols, int tlrow, int tlcol, int brrow, int brcol);
void undo_hide_show(int row, int col, char type, int arg);
void add_undo_col_format(int col, int type, int fwidth, int precision, int realfmt);

void add_to_undolist(struct undo u);
void do_undo();
void do_redo();

void clear_undo_list ();
void clear_from_current_pos();
int len_undo_list();
void free_undo_node(struct undo * ul);
void dismiss_undo_item(struct undo * ul);
struct ent * add_undo_aux_ent(struct ent * e);
