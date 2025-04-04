#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "upderiver.h"

static const int   L_TRUE       =  1;
static const int   L_FALSE      = -1;
static const int   L_UNDEF      =  0;
static const int   END_OF_WLIST = -1;
static const int   NO_REASON    = -1;
static const bool  READABLE     = true;
static const char* NO_HEADER    = NULL;

struct st_clause;
typedef struct st_clause clause;

STATIC inline int  lit_neg (upderiver *deriver, int lit);
STATIC inline int  lit_sign (upderiver *deriver, int lit);
STATIC inline int  lit_intvar (upderiver *deriver, int lit);
STATIC inline int  intvar_toLit (upderiver *deriver, int intvar);
STATIC inline int model_int (upderiver *deriver,
    int model[], int model_size, int intvar);

STATIC inline int  lit2val (upderiver *deriver, int lit);
STATIC inline bool lit_is_satisfied (upderiver *deriver, int lit);
STATIC inline bool lit_is_falsified (upderiver *deriver, int lit);

STATIC inline int *clause_lits  (upderiver *deriver, int cls);
STATIC inline int  clause_size  (upderiver *deriver, int cls);
STATIC inline int *clause_watch (upderiver *deriver, int cls);
STATIC inline int *clause_next  (upderiver *deriver, int cls);
STATIC int clause_import (upderiver *deriver, int *begin, int *end);

STATIC inline int  wlist_sentinel (upderiver *deriver, int lit);
STATIC inline int  wlist_first (upderiver *deriver, int lit);
STATIC inline int  wlist_next (upderiver *deriver, int cls, int lit);
STATIC inline void wlist_push (upderiver *deriver, int cls, int lit);
STATIC inline void wlist_change_next
    (upderiver *deriver, int prev, int from_lit, int to_lit);

STATIC int import_extvar (upderiver *deriver, int extvar);
STATIC inline void enqueue (upderiver *deriver, int lit, int cls);
STATIC inline void assume (upderiver *deriver, int lit);
STATIC inline void canceluntil (upderiver *deriver, int level);
STATIC int derive_clause (upderiver *deriver, int *clause, int maxlen);
STATIC int propagate (upderiver *deriver);

STATIC void fprintlit  (FILE *out, upderiver *deriver, int lit, bool readable);
STATIC void fprintlits (FILE *out, upderiver *deriver, 
    int *begin, int *end, bool readable, const char header[]);
STATIC void fprintlits_nl (FILE *out, upderiver *deriver, 
    int *begin, int *end, bool readable, const char header[]);
STATIC void fprintassigns (FILE *out, upderiver *deriver, 
    int *begin, int *end, bool readable, const char header[]);
STATIC void fprintassigns_nl (FILE *out, upderiver *deriver, 
    int *begin, int *end, bool readable, const char header[]);
STATIC void fprintclause (FILE *out, upderiver *deriver, 
    int cls, bool readable, const char header[]);
STATIC void fprintclause_nl (FILE *out, upderiver *deriver, 
    int cls, bool readable, const char header[]);
STATIC int  track_propagation (FILE *out, upderiver *deriver, 
    int level, int *sources, int *targets, int maxlen);
STATIC void fprint_propagation (FILE *out, upderiver *deriver, 
    int level, bool readable, const char header[]);

#ifndef NDEBUG
STATIC bool is_intvar (upderiver *deriver, int intvar);
STATIC bool is_clause (upderiver *deriver, int cls);
STATIC bool clause_is_watched_by (upderiver *deriver, int cls, int lit);
STATIC bool clause_is_sentinel (upderiver *deriver, int cls);
STATIC void print_not_watched_error (upderiver *deriver, int cls, int lit);
STATIC bool clause_is_unit (upderiver *deriver, int cls);
STATIC int* trail_begin (upderiver *deriver);
STATIC int  trail_size (upderiver *deriver);
STATIC int* trail_begin_at_level (upderiver *deriver, int level);
STATIC int  dlevel (upderiver *deriver);
STATIC bool upderiver_is_ivar (upderiver *deriver, int extvar);
STATIC bool upderiver_is_ovar (upderiver *deriver, int extvar);
STATIC bool upderiver_is_wvar (upderiver *deriver, int extvar);
STATIC const char *get_name (upderiver *deriver, int extvar);
#endif

struct st_clause {
    int  size;
    int  next[2];
    int  watch[2];
    int* lits;
};

struct upderiver_t
{
    int*    intvars; // internal variables
    int*    extvars; // external variables
    int     extsize; // number of external variables
    int     extcap;
    int*    model_ext;

    int     size;  // number of internal variables
    int     cap;
    int     qhead;
    int     qtail;
    int*    trail;
    int*    assigns;
    int*    reasons;
    int*    trail_lim;
    int     dlevel;

    int*    ivars; // input variables
    int*    ovars; // output variables
    int*    wvars; // witness variables
    char**  names; // variables names
    bool*   is_ivar;
    bool*   is_ovar;
    bool*   is_wvar;

    int     nof_ivars;
    int     nof_ovars;
    int     nof_wvars;

    int*    buf;
    int     buf_size; // initialize and clear buf_size each time buf is used.

    int     nof_clauses; // number of added clauses plus sentinels
    int     cap_clauses;
    clause* clauses;
    int*    wlists;      // linked lists of clauses watched by each literal

    // external functions to manipulate literals and variables
    int (*lit_neg)      (int lit);
    int (*lit_sign)     (int lit);
    int (*lit_extvar)   (int lit);
    int (*extvar_toLit) (int var);
};

//=============================================================================

STATIC inline int lit_neg (upderiver *deriver, int lit)
{
    if (NULL == deriver->lit_neg) {
        fprintf(stderr, "%sset %s with upderiver_addfunc().\n",
            upderiver_error_header,
            __func__);
        exit(EXIT_FAILURE);
    }
    return deriver->lit_neg(lit);
}

STATIC inline int lit_sign (upderiver *deriver, int lit)
{
    if (NULL == deriver->lit_sign) {
        fprintf(stderr, "%sset %s with upderiver_addfunc().\n",
            upderiver_error_header,
            __func__);
        exit(EXIT_FAILURE);
    }
    return deriver->lit_sign(lit);
}

STATIC inline int lit_intvar (upderiver *deriver, int lit)
{
    if (NULL == deriver->lit_extvar) {
        fprintf(stderr, "%sset %s with upderiver_addfunc().\n",
            upderiver_error_header,
            __func__);
        exit(EXIT_FAILURE);
    }
    int extvar = deriver->lit_extvar(lit);
    assert(extvar < deriver->extsize);
    return deriver->intvars[extvar];
}

STATIC inline int intvar_toLit (upderiver *deriver, int intvar)
{
    if (NULL == deriver->extvar_toLit) {
        fprintf(stderr, "%sset %s with upderiver_addfunc().\n",
            upderiver_error_header,
            __func__);
        exit(EXIT_FAILURE);
    }
    assert(0 <= intvar);
    assert(intvar < deriver->size);
    int extvar = deriver->extvars[intvar];
    return deriver->extvar_toLit(extvar);
}

STATIC inline int model_int (upderiver *deriver,
    int model[], int model_size, int intvar)
{
    assert(0 <= intvar);
    assert(intvar < deriver->size);
    int extvar = deriver->extvars[intvar];
    assert(extvar < model_size);
    int res = model[extvar];
    if (L_UNDEF == res) {
        fprintf(stderr, "%sno model set.\n",
            upderiver_error_header);
        exit(EXIT_FAILURE);
    }
    return res;
}

//=============================================================================

STATIC inline int lit2val (upderiver *deriver, int lit)
{
    return lit_sign(deriver, lit)? L_FALSE : L_TRUE;
}

STATIC inline bool lit_is_satisfied (upderiver *deriver, int lit)
{
    return lit2val(deriver, lit) == deriver->assigns[lit_intvar(deriver, lit)];
}

STATIC inline bool lit_is_falsified (upderiver *deriver, int lit)
{
    return lit2val(deriver, lit_neg(deriver, lit)) 
        == deriver->assigns[lit_intvar(deriver, lit)];
}

//=============================================================================

STATIC inline int *clause_lits (upderiver *deriver, int cls)
{
    assert(is_clause(deriver, cls));
    return deriver->clauses[cls].lits;
}

STATIC inline int clause_size (upderiver *deriver, int cls)
{
    assert(is_clause(deriver, cls));
    return deriver->clauses[cls].size;
}

STATIC inline int *clause_watch (upderiver *deriver, int cls)
{
    assert(is_clause(deriver, cls));
    return deriver->clauses[cls].watch;
}

STATIC inline int *clause_next (upderiver *deriver, int cls)
{
    assert(is_clause(deriver, cls));
    return deriver->clauses[cls].next;
}

STATIC int clause_import (upderiver *deriver, int *begin, int *end)
{
    if (end-begin <= 0) {
        fprintf(stderr, "%s%s received empty clause.\n", 
            upderiver_error_header,
            __func__);
        exit(EXIT_FAILURE);
    }
    if (end-begin == 1) {
        fprintf(stderr, "%s%s received unit clause.\n", 
            upderiver_error_header,
            __func__);
        exit(EXIT_FAILURE);
    }

    if (deriver->cap_clauses <= deriver->nof_clauses+1) {

        while (deriver->cap_clauses <= deriver->nof_clauses+1)
            deriver->cap_clauses = deriver->cap_clauses*2+1;

        deriver->clauses  = (clause*)realloc(
            deriver->clauses, 
            sizeof(clause)*(size_t)deriver->cap_clauses);

        for (int i = deriver->nof_clauses; i < deriver->cap_clauses; i++){
            deriver->clauses[i].size  = 0;
            deriver->clauses[i].next[0] = END_OF_WLIST;
            deriver->clauses[i].next[1] = END_OF_WLIST;
            deriver->clauses[i].lits  = NULL;
        }
    }
    assert(deriver->nof_clauses+1 < deriver->cap_clauses);

    deriver->nof_clauses++;
    const int cls = deriver->nof_clauses - 1;
    clause *p = deriver->clauses + cls;
    p->size = end-begin;
    p->lits = (int*)malloc(sizeof(int)*(size_t)p->size);
    for (int i = 0; i < p->size; i++)
        p->lits[i] = begin[i];
    p->watch[0] = lit_neg(deriver, begin[0]);
    p->watch[1] = lit_neg(deriver, begin[1]);
    return cls;
}

//=============================================================================

STATIC inline int wlist_sentinel (upderiver *deriver, int lit)
{
    assert(is_intvar(deriver, lit_intvar(deriver, lit)));
    const int intvar = lit_intvar(deriver, lit);
    return deriver->wlists[intvar];
}

STATIC inline int wlist_first (upderiver *deriver, int lit)
{
    assert(is_intvar(deriver, lit_intvar(deriver, lit)));
    int cls = wlist_sentinel(deriver, lit);
    return wlist_next(deriver, cls, lit);
}

STATIC inline int wlist_next (upderiver *deriver, int cls, int lit)
{
    assert(is_intvar(deriver, lit_intvar(deriver, lit)));
    assert(is_clause(deriver, cls));
#ifndef NDEBUG
    if (!clause_is_watched_by(deriver, cls, lit)) {
        print_not_watched_error(deriver, cls, lit);
        exit(EXIT_FAILURE);
    }
#endif
    for (int i = 0; i < 2; i++)
        if (lit == clause_watch(deriver, cls)[i])
            return clause_next(deriver, cls)[i];

    fprintf(stderr, "%s%s could not find next clause.\n", 
        upderiver_error_header,
        __func__);
    exit(EXIT_FAILURE);
}

STATIC inline void wlist_push (upderiver *deriver, int cls, int lit)
{
    assert(is_intvar(deriver, lit_intvar(deriver, lit)));
    assert(is_clause(deriver, cls));
    assert(!clause_is_sentinel(deriver, cls));

#ifndef NDEBUG
    if (!clause_is_watched_by(deriver, cls, lit)) {
        print_not_watched_error(deriver, cls, lit);
        exit(EXIT_FAILURE);
    }
#endif

    // add current clause to wlist of lit.
    const int sentinel = wlist_sentinel(deriver, lit);
    int next_to_sentinel = -1;
    for (int i = 0; i < 2; i++)
        if (lit == clause_watch(deriver,sentinel)[i]) {
            next_to_sentinel = clause_next(deriver,sentinel)[i];
            deriver->clauses[sentinel].next[i] = cls;
        }

    // update next field of current clause
    for (int i = 0; i < 2; i++)
        if (lit == clause_watch(deriver, cls)[i])
            deriver->clauses[cls].next[i] = next_to_sentinel;

#ifndef NDEBUG
    assert(wlist_next(deriver, sentinel, lit) == cls);
    assert(wlist_next(deriver, cls, lit)      == next_to_sentinel);
#endif
}

// change next clause of prev so that it gets watched by to_lit.
STATIC inline void wlist_change_next (upderiver *deriver, 
    int prev, int from_lit, int to_lit)
{
    assert(is_clause(deriver, prev));
    assert(is_intvar(deriver, lit_intvar(deriver, from_lit)));
    assert(is_intvar(deriver, lit_intvar(deriver, to_lit)));
    int curr = wlist_next(deriver, prev, from_lit);
#ifndef NDEBUG
    {
        bool found = false;
        int *begin = clause_lits(deriver, curr);
        int *end   = begin + clause_size(deriver, curr);
        for (int *i = begin; i < end; i++)
            if (lit_neg(deriver, to_lit) == *i) {
                found = true;
                break;
            }
        assert(found);
    }
#endif

    // remove current clause from wlist of from_lit.
    const int next = wlist_next(deriver, curr, from_lit);
    for (int i = 0; i < 2; i++)
        if (from_lit == clause_watch(deriver, prev)[i])
            deriver->clauses[prev].next[i] = next;
    assert(wlist_next(deriver, prev, from_lit) == next);

    // update watch of current clause
    for (int i = 0; i < 2; i++)
        if (from_lit == clause_watch(deriver, curr)[i])
            deriver->clauses[curr].watch[i] = to_lit;

    // add current clause to wlist of to_lit.
    wlist_push(deriver, curr, to_lit);
}

//=============================================================================

STATIC int import_extvar_main (upderiver *deriver, int extvar)
{
    assert(extvar < deriver->extsize);
    if (0 <= deriver->intvars[extvar])
        return deriver->intvars[extvar];

    const int intvar = deriver->size;
    deriver->size++;
    deriver->intvars[extvar] = intvar;
    
    // reallocate each data structure if necessary to set intvar.
    if (intvar >= deriver->cap){
        while (intvar >= deriver->cap)
          deriver->cap = deriver->cap*2+1;

        deriver->extvars  = (int*)realloc(deriver->extvars,
            sizeof(int)*(size_t)deriver->cap);
        deriver->wlists   = (int*)realloc(deriver->wlists,
            sizeof(int)*(size_t)deriver->cap);
        deriver->is_ovar  = (bool*)realloc(deriver->is_ovar,
            sizeof(bool)*(size_t)deriver->cap);
        deriver->is_ivar = (bool*)realloc(deriver->is_ivar,
            sizeof(bool)*(size_t)deriver->cap);
        deriver->is_wvar  = (bool*)realloc(deriver->is_wvar,
            sizeof(bool)*(size_t)deriver->cap);
        deriver->wvars = (int*) realloc(deriver->wvars,
            sizeof(int)*(size_t)deriver->cap);
        deriver->ivars = (int*) realloc(deriver->ivars,
            sizeof(int)*(size_t)deriver->cap);
        deriver->ovars = (int*) realloc(deriver->ovars,
            sizeof(int)*(size_t)deriver->cap);
        deriver->assigns  = (int*) realloc(deriver->assigns,
            sizeof(int)*(size_t)deriver->cap);
        deriver->reasons  = (int*) realloc(deriver->reasons,
            sizeof(int)*(size_t)deriver->cap);
        deriver->trail    = (int*) realloc(deriver->trail,
            sizeof(int)*(size_t)deriver->cap);
        deriver->names    = (char**)realloc(deriver->names,
            sizeof(char*)*(size_t)deriver->cap);
        deriver->trail_lim = (int*) realloc(deriver->trail_lim,
            sizeof(int)*(size_t)deriver->cap);
        deriver->buf      = (int*) realloc(deriver->buf,
            sizeof(int)*(size_t)deriver->cap);
    }
    assert(intvar < deriver->cap);
    assert(intvar+1 == deriver->size);

    deriver->extvars [intvar] = extvar;
    deriver->is_ivar [intvar] = false;
    deriver->is_ovar [intvar] = false;
    deriver->is_wvar [intvar] = false;
    deriver->assigns [intvar] = L_UNDEF;
    deriver->reasons [intvar] = NO_REASON;
    deriver->names   [intvar] = NULL;

    int lits[2];
    lits[0] = deriver->extvar_toLit(extvar);
    lits[1] = deriver->lit_neg(deriver->extvar_toLit(extvar));
    deriver->wlists[intvar] = clause_import(deriver, lits, lits+2); // sentinel
    assert(is_clause(deriver, deriver->wlists[intvar]));

    return intvar;
}

STATIC int import_extvar (upderiver *deriver, int extvar)
{
    assert(extvar >= 0);
    // reallocate intvars if necessary to set extvar.
    const int extn = extvar+1;
    if (deriver->extcap < extn){
        while (deriver->extcap < extn)
          deriver->extcap = deriver->extcap*2+1;
        deriver->intvars = (int*)realloc(deriver->intvars, 
            sizeof(int)*(size_t)deriver->extcap);
        deriver->model_ext = (int*)realloc(deriver->model_ext, 
            sizeof(int)*(size_t)deriver->extcap);
    }
    for (int i = deriver->extsize; i < extn; i++) {
        deriver->intvars[i]   = -1;
        deriver->model_ext[i] = L_UNDEF;
    }
    deriver->extsize = extn > deriver->extsize ? extn : deriver->extsize;
    assert(extn <= deriver->extsize);

    return import_extvar_main(deriver, extvar);
}

STATIC inline void enqueue (upderiver *deriver, int lit, int cls)
{
    assert(is_intvar(deriver, lit_intvar(deriver, lit)));
    assert(NO_REASON == cls 
        || (is_clause(deriver, cls) && !clause_is_sentinel(deriver, cls)));

    if (lit_is_falsified(deriver, lit)) {
        fprintf(stderr, "%sfailed to enqueue ", 
            upderiver_error_header);
        fprintlits_nl(stderr, deriver, &lit, &lit+1, READABLE, NO_HEADER);
        exit(EXIT_FAILURE);
    }

    if (lit_is_satisfied(deriver, lit))
        return;

    const int intvar = lit_intvar(deriver, lit);
    assert (L_UNDEF == deriver->assigns[intvar]);
    deriver->assigns[intvar] = lit2val(deriver, lit);
    deriver->trail[deriver->qtail++] = lit;
    deriver->reasons[intvar] = cls;
    assert(lit_is_satisfied(deriver, lit));
}


STATIC inline void assume (upderiver *deriver, int lit)
{
    assert(is_intvar(deriver, lit_intvar(deriver, lit)));
    if (deriver->qtail != deriver->qhead) {
        fprintf(stderr, "%s%s invoked without propagation.\n",
            upderiver_error_header,
            __func__);
        exit(EXIT_FAILURE);
    }
    if (lit_is_falsified(deriver, lit)) {
        fprintf(stderr, "%sfailed to assume ", 
            upderiver_error_header);
        fprintlits_nl(stderr, deriver, &lit, &lit+1, READABLE, NO_HEADER);
        exit(EXIT_FAILURE);
    }
    if (lit_is_satisfied(deriver, lit))
        return;

    assert(deriver->dlevel < deriver->size);
    deriver->trail_lim[deriver->dlevel++] = deriver->qtail;
    enqueue(deriver, lit, NO_REASON);
}

STATIC inline void canceluntil (upderiver *deriver, int level)
{
    if (deriver->dlevel <= level)
        return;
    level = level < 0? 0: level; 
    const int bound = deriver->trail_lim[level];
    for (int pos = deriver->qtail-1; pos >= bound; pos--) {
        const int lit = deriver->trail[pos];
        deriver->assigns[lit_intvar(deriver, lit)] = L_UNDEF;
        deriver->reasons[lit_intvar(deriver, lit)] = NO_REASON;
    }

    deriver->qhead = deriver->qtail = bound;
    deriver->dlevel = level;
}

STATIC int derive_clause (upderiver *deriver, int *clause, int maxlen)
{
    int count = 0;
    const int bound = deriver->trail_lim[deriver->dlevel-1];
    for (int pos = deriver->qtail-1; pos >= bound; pos--) {
        const int lit = deriver->trail[pos];
        if (deriver->is_ivar[lit_intvar(deriver, lit)]) {
            if (!(count < maxlen)) {
                fprintf(stderr, "%s%s exceeds maximum length.\n",
                        upderiver_error_header,
                        __func__);
                exit(EXIT_FAILURE);
            }
            clause[count++] = lit_neg(deriver,lit);
        }
    }

    return count;
}

STATIC int propagate (upderiver *deriver)
{
    int count = 0; // number of unit clauses

    while (deriver->qtail - deriver->qhead > 0){
        const int lit = deriver->trail[deriver->qhead++];
        int prev = wlist_sentinel(deriver, lit);
        int curr = wlist_first(deriver, lit);
        while (0 <= curr) {
            // negation of the other watch
            const int neg_w_prime = lit == clause_watch(deriver, curr)[0]?
                                lit_neg(deriver, clause_watch(deriver, curr)[1]):
                                lit_neg(deriver, clause_watch(deriver, curr)[0]);
            assert(lit_is_falsified(deriver, lit_neg(deriver, lit)));

            if (lit_is_satisfied(deriver, neg_w_prime)) {
                assert(!clause_is_unit(deriver, curr));
                prev = curr;
                curr = wlist_next(deriver, curr, lit);
            } else {
                int *begin = clause_lits(deriver, curr);
                int *end   = begin + clause_size(deriver, curr);
                bool found = false; 
                for (int *i = begin; i < end; i++){
                    // if new watch found:
                    if (neg_w_prime != *i && !lit_is_falsified(deriver, *i)){
                        found = true;
                        int next = wlist_next(deriver, curr, lit);
                        // remove current clause from wlist of lit and 
                        // push it to wlist of new watch.
                        wlist_change_next(deriver,
                            prev, lit, lit_neg(deriver, *i));
                        // prev is unchanged.
                        assert(wlist_next(deriver, prev, lit) == next);
                        curr = next;
                        break;
                    }
                }

                // unit propagation:
                if (!found) {
                    assert(clause_is_unit(deriver, curr));
                    enqueue(deriver, neg_w_prime, curr);
                    count++;
                    prev = curr;
                    curr = wlist_next(deriver, curr, lit);
                }
            }
        }
    }

#ifndef NDEBUG
    assert(deriver->qhead == deriver->qtail);
    for (int intvar = 0; intvar < deriver->size; intvar++) {
        int lits[2] = {
            intvar_toLit(deriver, intvar), 
            lit_neg(deriver, intvar_toLit(deriver, intvar))};
        for (int i = 0; i < 2; i++)
            for (int cls = wlist_first(deriver, lits[i]);
                0 <= cls; cls = wlist_next(deriver, cls, lits[i]))
                if (clause_is_unit(deriver, cls)) {
                    fprintf(stderr, "%s%s did not propagate unit clause ",
                        upderiver_error_header,
                        __func__);
                    fprintclause_nl(stderr, deriver, cls, READABLE, NO_HEADER);
                    exit(EXIT_FAILURE);
                }
    }
#endif

    return count;
}

//=============================================================================

upderiver* upderiver_new (void)
{
    upderiver* deriver = (upderiver*)malloc(sizeof(upderiver));

    deriver->intvars = NULL;
    deriver->extvars = NULL;
    deriver->extsize = 0;
    deriver->extcap  = 0;
    deriver->model_ext = NULL;

    deriver->size   = 0;
    deriver->cap    = 0;
    deriver->qhead  = 0;
    deriver->qtail  = 0;
    deriver->dlevel = 0;

    deriver->trail     = NULL;
    deriver->assigns   = NULL;
    deriver->reasons   = NULL;
    deriver->trail_lim = NULL;

    deriver->is_ivar  = NULL;
    deriver->is_ovar  = NULL;
    deriver->is_wvar  = NULL;
    deriver->ivars = NULL;
    deriver->ovars = NULL;
    deriver->wvars = NULL;
    deriver->names    = NULL;

    deriver->nof_ivars = 0;
    deriver->nof_ovars = 0;
    deriver->nof_wvars = 0;

    deriver->buf      = NULL;
    deriver->buf_size = 0;

    deriver->nof_clauses = 0;
    deriver->cap_clauses = 0;
    deriver->clauses     = NULL;

    deriver->wlists   = NULL;

    deriver->lit_neg      = NULL;
    deriver->lit_sign     = NULL;
    deriver->lit_extvar   = NULL;
    deriver->extvar_toLit = NULL;

    return deriver;
}

void upderiver_delete (upderiver *deriver)
{ 
    if (NULL != deriver->intvars) {
        free(deriver->intvars);
        free(deriver->model_ext);
    }

    if (NULL != deriver->extvars){
        free(deriver->extvars);
        free(deriver->trail);
        free(deriver->assigns);
        free(deriver->reasons);
        free(deriver->trail_lim);
        free(deriver->is_ivar);
        free(deriver->is_ovar);
        free(deriver->is_wvar);
        free(deriver->ivars);
        free(deriver->ovars);
        free(deriver->wvars);

        for (int i = 0; i < deriver->size; i++) {
            char *p = deriver->names[i];
            if (NULL != p) free(p);
        }
        free(deriver->names);

        free(deriver->buf);
        free(deriver->wlists);
    }

    if (NULL != deriver->clauses) {
        clause *begin = deriver->clauses;
        clause *end   = begin + deriver->nof_clauses;
        for (clause *p = begin; p < end; p++) 
            free(p->lits);
        free(deriver->clauses);
    }

    free(deriver);
}

void upderiver_addfunc (upderiver *deriver,
    int (*lit_neg)      (int lit),
    int (*lit_sign)     (int lit),
    int (*lit_var)      (int lit),
    int (*toLit)        (int var))
{
    deriver->lit_neg      = lit_neg;
    deriver->lit_sign     = lit_sign;
    deriver->lit_extvar   = lit_var;
    deriver->extvar_toLit = toLit;
}

int upderiver_addclause (upderiver *deriver, int *begin, int *end)
{
    if (0 != deriver->dlevel) {
        fprintf(stderr, "%s%s was called in non-root level.\n",
            upderiver_error_header,
            __func__);
        exit(EXIT_FAILURE);
    }
    if (end-begin < 1) {
        fprintf(stderr, "%s%s received empty clause.\n", 
            upderiver_error_header,
            __func__);
        exit(EXIT_FAILURE);
    }
    for (int* i = begin; i < end; i++) {
        int *min_j = i;
        for (int *j = i+1; j < end; j++) {
            min_j = deriver->lit_extvar(*j) < deriver->lit_extvar(*min_j) 
                || (deriver->lit_extvar(*j) == deriver->lit_extvar(*min_j) 
                    && lit_neg(deriver, *j))? j: min_j;
        }
        int tmp = *i;
        *i      = *min_j;
        *min_j  = tmp;
    }
    for (int* i = begin; i+1 < end; i++) {
        int* new_end = i+1;
        for (int* j = i+1; j < end; j++) {
            if (lit_neg(deriver, *j) == *i) // tautology
                return upderiver_tautology_was_added;
            if (*i == *j) // duplicates
                continue;
            int tmp = *new_end;
            *new_end = *j;
            *j = tmp;
            new_end++;
        }
        end = new_end;
    }

    for (int* i = begin; i < end; i++)
        import_extvar(deriver, deriver->lit_extvar(*i));

    assert (end-begin >= 1);
    if (end-begin == 1) {
        enqueue(deriver, begin[0], NO_REASON);
        return upderiver_unit_was_added;
    } else {
        const int cls = clause_import(deriver, begin, end);
        wlist_push(deriver, cls, clause_watch(deriver,cls)[0]);
        wlist_push(deriver, cls, clause_watch(deriver,cls)[1]);
        return cls;
    }
}

STATIC void setivar (upderiver *deriver, int extvar)
{
    assert(0 <= extvar);
    const int intvar = import_extvar(deriver, extvar);

    if (deriver->is_wvar[intvar] || deriver->is_ovar[intvar]) {
        fprintf(stderr, "%s%s could not set %d\n", 
            upderiver_error_header,
            __func__,
            extvar);
        exit(EXIT_FAILURE);
    }

    if (false == deriver->is_ivar[intvar])
        deriver->ivars[deriver->nof_ivars++] = intvar;
    deriver->is_ivar[intvar] = true;
}

STATIC void setovar (upderiver *deriver, int extvar)
{
    assert(0 <= extvar);
    const int intvar = import_extvar(deriver, extvar);

    if (deriver->is_wvar[intvar] || deriver->is_ivar[intvar]) {
        fprintf(stderr, "%s%s could not set %d\n", 
            upderiver_error_header,
            __func__,
            extvar);
        exit(EXIT_FAILURE);
    }

    if (false == deriver->is_ovar[intvar])
        deriver->ovars[deriver->nof_ovars++] = intvar;
    deriver->is_ovar[intvar] = true;
}

STATIC void setwvar (upderiver *deriver, int extvar)
{
    assert(0 <= extvar);
    const int intvar = import_extvar(deriver, extvar);

    if (deriver->is_ovar[intvar] || deriver->is_ivar[intvar]) {
        fprintf(stderr, "%s%s could not set %d\n", 
            upderiver_error_header,
            __func__,
            extvar);
        exit(EXIT_FAILURE);
    }

    if (false == deriver->is_wvar[intvar])
        deriver->wvars[deriver->nof_wvars++] = intvar;
    deriver->is_wvar[intvar] = true;
}

// n: number of characters including '\0'
STATIC void setname (upderiver *deriver,
    char *name, int n, int extvar)
{
    assert(0 <= extvar);
    if (n <= 1) {
        fprintf(stderr, "%s%s received empty string.\n",
            upderiver_error_header,
            __func__);
        exit(EXIT_FAILURE);
    }
    if (name[n-1] != '\0') {
        fprintf(stderr, "%s%s received string missing null character.\n",
            upderiver_error_header,
            __func__);
        exit(EXIT_FAILURE);
    }
    char *copy = (char*)malloc(sizeof(char)*(size_t)n);
    int i = 0;
    while (i < n) {
        copy[i] = name[i];
        i++;
    }
    assert(copy[n-1] == '\0');

    const int intvar = import_extvar(deriver, extvar);
    if (NULL == deriver->names[intvar])
        deriver->names[intvar] = copy;
    else {
        fprintf(stderr, "%svariable %d already has name.\n", 
            upderiver_warning_header, extvar);
        free(copy);
    }
}

void upderiver_setvar (upderiver *deriver,
    int var, upderiver_var_tag tag, char* name, int maxlen)
{
    if      (CECD_IVAR == tag)
        setivar (deriver, var);
    else if (CECD_OVAR == tag)
        setovar (deriver, var);
    else if (CECD_WVAR == tag)
        setwvar (deriver, var);
    else {
        fprintf(stderr, "%s%s received invalid variable tag\n",
            upderiver_error_header,
            __func__
        );
        exit(EXIT_FAILURE);
    }
    setname (deriver, name, maxlen, var);
}

int  upderiver_derive (upderiver *deriver,
    int derived_clause[], int max_derived_clause_size, int *min_i,
    int falsified_ovars[], int nof_falsified_ovars,
    int model[],  int model_size)
{
    if (NULL == falsified_ovars || NULL == derived_clause) {
        fprintf(stderr, "%s%s received null pointer.\n",
            upderiver_error_header,
            __func__);
        exit(EXIT_FAILURE);
    }
    if (nof_falsified_ovars <= 0) {
        fprintf(stderr, "%s%s received no output variable.\n",
            upderiver_error_header,
            __func__);
        exit(EXIT_FAILURE);
    }
    if (nof_falsified_ovars > 1) {
        fprintf(stderr,
            "%s%s multiple output variables not supported in current version.\n",
            upderiver_error_header,
            __func__);
        exit(EXIT_FAILURE);
    }
    if (deriver->dlevel != 0) {
        fprintf(stderr, "%s%s must be called at root level.\n",
            upderiver_error_header,
            __func__);
        exit(EXIT_FAILURE);
    }

    const int intvar = deriver->intvars[falsified_ovars[0]];
    assert(deriver->is_ovar[intvar]);
    assert(is_intvar(deriver, intvar));
    assert(deriver->assigns[intvar] == L_UNDEF);
    assume(deriver, lit_neg(deriver, intvar_toLit(deriver, intvar)));
    propagate(deriver);
    for (int i = 0; i < deriver->nof_wvars; i++) {
        const int intvar = deriver->wvars[i];
        assert(deriver->is_wvar[intvar]);
        enqueue(deriver,
            model_int(deriver, model, model_size, intvar) == L_TRUE?
            intvar_toLit(deriver, intvar):
            lit_neg(deriver, intvar_toLit(deriver, intvar)),
            NO_REASON);
        propagate(deriver);
    }
    const int derived_clause_size = 
        derive_clause(deriver, &derived_clause[0], deriver->size);
    assert(derived_clause_size <= deriver->size);
    canceluntil(deriver, 0);

    /*
    fprintf(stdout, "c derived(%d) ", derived_clause_size);
    fprintlits_nl(stdout, deriver, 
        derived_clause, derived_clause+derived_clause_size, READABLE, NO_HEADER);
    */

    return derived_clause_size;
}

//=============================================================================

STATIC void fprintlit (FILE *out, upderiver *deriver, int lit, bool readable)
{
    const int intvar = lit_intvar(deriver, lit);
    const int extvar = deriver->extvars[intvar];
    if (!readable) {
        fprintf(out, "%s%d", lit_sign(deriver, lit)? "~": "", extvar);
        return;
    }

    fprintf(out, "%s", lit_sign(deriver, lit)? "~":"");
    if (NULL != deriver->names[intvar])
        fprintf(out, "%s", deriver->names[intvar]);
    else
        fprintf(out, "%d", extvar);
}

STATIC void fprintlits (FILE *out, upderiver *deriver, 
    int *begin, int *end, bool readable, const char header[])
{
    fprintf(out, "%s", header == NO_HEADER? "": header);
    for (int* i = begin; i < end; i++) {
        fprintlit(out, deriver, *i, readable);
        if (i+1 < end)
            fprintf(out, " ");
    }
}

STATIC void fprintlits_nl (FILE *out, upderiver *deriver, 
    int *begin, int *end, bool readable, const char header[])
{
    fprintlits(out, deriver, begin, end, readable, header);
    fprintf(out, "\n");
}

STATIC void fprintassigns (FILE *out, upderiver *deriver, 
    int *begin, int *end, bool readable, const char header[])
{
    fprintf(out, "%s", header == NO_HEADER? "": header);
    for (int* i = begin; i < end; i++) {
        int intvar = lit_intvar(deriver, *i);
        fprintlit(out, deriver, intvar_toLit(deriver, intvar), readable);
        fprintf(out, "=");
        fprintf(out, "%d", deriver->assigns[intvar]);
        if (i+1 < end)
            fprintf(out, " ");
    }
}

STATIC void fprintassigns_nl (FILE *out, upderiver *deriver, 
    int *begin, int *end, bool readable, const char header[])
{
    fprintassigns(out, deriver, begin, end, readable, header);
    fprintf(out, "\n");
}

STATIC void fprintclause (FILE *out, upderiver *deriver, 
    int cls, bool readable, const char header[])
{
    assert(is_clause(deriver, cls));
    fprintf(out, "%s", header == NO_HEADER? "": header);
    int *begin = clause_lits(deriver, cls);
    int *end   = begin + clause_size(deriver, cls);
    for (int* i = begin; i < end; i++) {
        fprintlit(out, deriver, *i, readable);
        if (lit_neg(deriver, clause_watch(deriver, cls)[0]) == *i
            || lit_neg(deriver, clause_watch(deriver, cls)[1]) == *i)
            fprintf(out, "*");
        if (i+1 < end)
            fprintf(out, " ");
    }

}

STATIC void fprintclause_nl (FILE *out, upderiver *deriver, 
    int cls, bool readable, const char header[])
{
    fprintclause(out, deriver, cls, readable, header);
    fprintf(out, "\n");
}

STATIC int track_propagation (FILE *out, upderiver *deriver, 
    int level, int *sources, int *targets, int maxlen)
{
    assert(sources != NULL);
    assert(targets != NULL);
    int count = 0;
    int begin = 0 < level? deriver->trail_lim[level-1]: 0;
    int end   = level < deriver->dlevel ? deriver->trail_lim[level]: deriver->qtail;
    for (int pos = begin; pos < end; pos++) {
        const int lit = deriver->trail[pos];
        const int intvar = lit_intvar(deriver, lit);
        int cls = deriver->reasons[intvar];
        if (0 > cls) // NO_REASON
            continue;
#ifndef NDEBUG
        for (int *i = clause_lits(deriver, cls);
            i < clause_lits(deriver, cls) + clause_size(deriver, cls); i++)
            if (lit != *i)
                assert(lit_is_falsified(deriver, *i));
#endif
        if (! (count < maxlen)) {
            fprintf(stderr, "%stoo short to track propagation.\n",
                upderiver_warning_header);
            return count;
        }
        assert(lit_neg(deriver, lit) == clause_watch(deriver, cls)[0]
            || lit_neg(deriver, lit) == clause_watch(deriver, cls)[1]);
        *(sources+count) = lit_neg(deriver, lit) == clause_watch(deriver, cls)[0]?
                            clause_watch(deriver, cls)[1]:
                            clause_watch(deriver, cls)[0];
        *(targets+count) = lit;
        count++;
    }
    return count; 
}

STATIC void fprint_propagation (FILE *out, upderiver *deriver, 
    int level, bool readable, const char header[])
{
    struct digraph {
        int source[BUFSIZ];
        int target[BUFSIZ];
        int size;
    };
    struct digraph g;
    g.size = track_propagation(out, deriver, level, g.source, g.target, BUFSIZ);

    for (int i = 0; i < g.size; i++) {
        int cls = deriver->reasons[lit_intvar(deriver, g.target[i])];

        fprintf(out, "%s", header == NO_HEADER? "": header);
        fprintlit(out, deriver, g.source[i], readable);
        fprintf(out, " > ");
        fprintlit(out, deriver, g.target[i], readable);
        fprintf(out, " : ");
        fprintclause_nl(out,
            deriver,
            cls,
            readable,
            NO_HEADER);
    }
}

//=============================================================================

#ifndef NDEBUG
STATIC bool is_intvar (upderiver *deriver, int intvar)
{
    return 0 <= intvar && intvar < deriver->size;
}

STATIC bool is_clause (upderiver *deriver, int cls)
{
    return 0 <= cls && cls < deriver->nof_clauses;
}

STATIC bool clause_is_watched_by (upderiver *deriver, int cls, int lit)
{
    assert(is_clause(deriver, cls));
    const int false_lit = lit_neg(deriver, lit);
    bool found = false;
    int *begin = clause_lits(deriver, cls);
    int *end   = begin + clause_size(deriver, cls);
    for (int *i = begin; i < end; i++) {
        if (false_lit == *i)
            found = true;
    }
    if (!found)
        return false;

    for (int i = 0; i < 2; i++)
        if (lit == clause_watch(deriver, cls)[i])
            return true;
    return false;
}

STATIC bool clause_is_sentinel (upderiver *deriver, int cls)
{
    assert(is_clause(deriver, cls));
    int lit = clause_watch(deriver, cls)[0];
    return wlist_sentinel(deriver, lit) == cls;
}

STATIC void print_not_watched_error (upderiver *deriver, int cls, int lit)
{
    assert(is_clause(deriver, cls));
    fprintclause(stderr,
        deriver,
        cls,
        READABLE,
        upderiver_error_header);
    fprintf(stderr, " not watched by ");
    fprintlit(stderr, deriver, lit, READABLE);
    fprintf(stderr, "\n");
}

STATIC bool clause_is_unit (upderiver *deriver, int cls)
{
    assert(is_clause(deriver, cls));
    int nof_true  = 0;
    int nof_false = 0;
    int nof_undef = 0;
    int *begin = clause_lits(deriver, cls);
    int *end   = begin + clause_size(deriver, cls);
    for (int *i = begin; i < end; i++) {
        if (lit_is_satisfied(deriver, *i))
            nof_true++;
        if (lit_is_falsified(deriver, *i))
            nof_false++;
        if (L_UNDEF == deriver->assigns[lit_intvar(deriver, *i)])
            nof_undef++;
    }
    assert(nof_true + nof_false + nof_undef == clause_size(deriver, cls));
    return nof_false +1 == clause_size(deriver, cls) && 1 == nof_undef;
}

STATIC int* trail_begin (upderiver *deriver)
{
    return deriver->trail;
}

STATIC int  trail_size (upderiver *deriver)
{
    return deriver->qtail;
}

STATIC int* trail_begin_at_level (upderiver *deriver, int level)
{
    assert(level <= dlevel(deriver));
    if (level <= 0)
        return trail_begin(deriver);
    
    int pos = deriver->trail_lim[level-1];
    assert(pos < trail_size(deriver));
    return deriver->trail + pos;
}

STATIC int  dlevel (upderiver *deriver)
{
    return deriver->dlevel;
}


STATIC bool upderiver_is_ivar (upderiver *deriver, int extvar)
{
    int intvar = deriver->intvars[extvar];
    return deriver->is_ivar[intvar];
}

STATIC bool upderiver_is_wvar (upderiver *deriver, int extvar)
{
    int intvar = deriver->intvars[extvar];
    return deriver->is_wvar[intvar];
}

STATIC bool upderiver_is_ovar (upderiver *deriver, int extvar)
{
    int intvar = deriver->intvars[extvar];
    return deriver->is_ovar[intvar];
}

STATIC const char *get_name (upderiver *deriver, int extvar)
{
    int intvar = deriver->intvars[extvar];
    return deriver->names[intvar];
}

#endif
