/* Adaptation of Chuck Martin's code - <nrocinu@myrealbox.com> */

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

#include "macros.h"
#include "yank.h"
#include "cmds.h"
#include "conf.h"
#include "color.h"
#include "xmalloc.h" // for scxfree

int compare(const void * row1, const void * row2);

struct sortcrit {
    int direction, type, column;
} * sort;

int howmany;

void sortrange(struct ent * left, struct ent * right, char * criteria) {
    int minr, minc, maxr, maxc, r, c;
    int * rows, col = 0;
    int cp = 0;

    minr = left->row < right->row ? left->row : right->row;
    minc = left->col < right->col ? left->col : right->col;
    maxr = left->row > right->row ? left->row : right->row;
    maxc = left->col > right->col ? left->col : right->col;

    sort = (struct sortcrit *) scxmalloc((2 * sizeof(struct sortcrit)));

    // Save 'ent' elements in the range to the 'rows' structure
    rows = (int *) scxmalloc((maxr - minr + 1) * sizeof(int));
    for (r = minr, c = 0; r <= maxr; r++, c++)
        rows[c] = r;

    if (! criteria) {
        sort[0].direction = 1;
        sort[0].type = 1;
        sort[0].column = minc;
        sort[1].direction = 1;
        sort[1].type = 0;
        sort[1].column = minc;
        howmany = 2;
    } else {
        howmany = 0;
        while (criteria[cp]) {
            if (howmany > 1)
                sort = (struct sortcrit *) scxrealloc((char *) sort,  (howmany + 1) * (sizeof(struct sortcrit)));

            switch (criteria[cp++]) {
                case '+':
                    sort[howmany].direction = 1;
                    break;
                case '-':
                    sort[howmany].direction = -1;
                    break;
                default:
                    sc_error("Invalid sort criteria");
                    return;
            }
            switch (criteria[cp++]) {
                case '#':
                    sort[howmany].type = 0;
                    break;
                case '$':
                    sort[howmany].type = 1;
                    break;
                default:
                    sc_error("Invalid sort criteria");
                    return;
            }
            if (criteria[cp]) {
                col = toupper(criteria[cp++]) - 'A';
            } else {
                sc_error("Invalid sort criteria");
                return;
            }
            if (criteria[cp] && criteria[cp] != '+' && criteria[cp] != '-' && criteria[cp] != ';')
                col = (col + 1) * 26 + toupper(criteria[cp++]) - 'A';
            sort[howmany].column = col;
            if (col < minc || col > maxc) {
                sc_error("Invalid sort criteria");
                return;
            }
            cp++;
            howmany++;
            if (cp > strlen(criteria))
                break;
        }
    }

    // Sort 'rows' structure
    qsort(rows, maxr - minr + 1, sizeof(int), compare);

    //currow = minr;
    //curcol = minc;

    yank_area(minr, minc, maxr, maxc, 's', 1); // save yanklist in the original range

    // Fix the 'ent' elements in the sorted range
    int i, d, move;
    struct ent * p_aux, * yl = get_yanklist();

    // Traverse 'rows' structure
    for (d = 0; d < (maxr - minr + 1); d++) {
        for (i = 0; i < (maxc - minc + 1); i++) {
            p_aux = yl;
            move = ( rows[d] - minr) * (maxc - minc + 1) + i;
            while (move--) p_aux = p_aux->next;
            p_aux->row = minr + d;
        }
    }

    currow = minr;
    curcol = minc;

    //paste_yanked_ents(0, 'a'); // paste ents over currow and curcol
    paste_yanked_ents(0, 's'); // paste ents over currow and curcol

    scxfree((char *) sort);
    scxfree((char *) rows);

    if (criteria) scxfree(criteria);
}

int compare(const void * row1, const void * row2) {
    struct ent * p1;
    struct ent * p2;
    double diff;
    int result = 0;
    int i;

    for (i = 0; !result && i < howmany; i++) {
        p1 = *ATBL(tbl, *((int *) row1), sort[i].column);
        p2 = *ATBL(tbl, *((int *) row2), sort[i].column);

        if (sort[i].type) {
            if (p1 && p1->label) {
                if (p2 && p2->label) {
                    result = strcmp(p1->label, p2->label);
                } else {
                    result = -1;
                }
            } else if (p2 && p2->label) {
                result = 1;
            }
        } else if (p1 && p2 && p1->flags & is_valid && p2->flags & is_valid) {
                diff = (p1->v - p2->v);
                result = (diff > 0 ? 1 : diff < 0 ? -1 : 0);
        } else if (p1 && p1->flags & is_valid) {
                result = -1;
        } else if (p2 && p2->flags & is_valid) {
            result = 1;
        }
        result *= sort[i].direction;
    }

    if (! result) result = (*((int *) row1) - *((int *) row2));

    return (result);
}
