#ifndef upderiver_h
#define upderiver_h
#ifdef _cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <string.h>
#include <assert.h>

#ifndef __TEST__
#define STATIC static
#else
#define STATIC
#endif

struct upderiver_t;
typedef struct upderiver_t upderiver;
typedef enum {
    CECD_IVAR,
    CECD_OVAR,
    CECD_WVAR,
} upderiver_var_tag;

extern upderiver* upderiver_new (void);
extern void upderiver_delete (upderiver *deriver);
extern void upderiver_addfunc (upderiver *deriver,
    int (*lit_neg)  (int lit),
    int (*lit_sign) (int lit),
    int (*lit_var)  (int lit),
    int (*toLit)    (int var));
extern int  upderiver_addclause (upderiver *deriver, int *begin, int *end);
extern void upderiver_setvar (upderiver *deriver,
    int var, upderiver_var_tag tag, char* name, int maxlen);
extern int  upderiver_derive (upderiver *deriver,
    int derived_clause[], int max_derived_clause_size,
    int *min_i,
    int falsified_ovars[],  int nof_falsified_ovars,
    int model[],  int model_size);

static const int upderiver_tautology_was_added = -2;
static const int upderiver_unit_was_added      = -1;

static const char upderiver_warning_header[] = "WARNING: ";
static const char upderiver_error_header[]   = "ERROR: ";

static const char upderiver_ivar_line_prefix[]  = "c IVAR ";
static const char upderiver_ovar_line_prefix[]  = "c OVAR ";
static const char upderiver_wvar_line_prefix[]  = "c WVAR ";

static inline bool upderiver_match_prefix (const char prefix[], char *in)
{
    return strncmp(prefix,in,strlen(prefix)) == 0;
}

static inline void upderiver_skip_prefix (const char prefix[], char **in)
{
    assert (upderiver_match_prefix(prefix,*in));
    *in += strlen(prefix);
}

#ifdef _cplusplus
}
#endif
#endif
