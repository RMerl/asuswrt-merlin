#include <string.h>
#include <jim-nvp.h>

int Jim_GetNvp(Jim_Interp *interp,
    Jim_Obj *objPtr, const Jim_Nvp * nvp_table, const Jim_Nvp ** result)
{
    Jim_Nvp *n;
    int e;

    e = Jim_Nvp_name2value_obj(interp, nvp_table, objPtr, &n);
    if (e == JIM_ERR) {
        return e;
    }

    /* Success? found? */
    if (n->name) {
        /* remove const */
        *result = (Jim_Nvp *) n;
        return JIM_OK;
    }
    else {
        return JIM_ERR;
    }
}

Jim_Nvp *Jim_Nvp_name2value_simple(const Jim_Nvp * p, const char *name)
{
    while (p->name) {
        if (0 == strcmp(name, p->name)) {
            break;
        }
        p++;
    }
    return ((Jim_Nvp *) (p));
}

Jim_Nvp *Jim_Nvp_name2value_nocase_simple(const Jim_Nvp * p, const char *name)
{
    while (p->name) {
        if (0 == strcasecmp(name, p->name)) {
            break;
        }
        p++;
    }
    return ((Jim_Nvp *) (p));
}

int Jim_Nvp_name2value_obj(Jim_Interp *interp, const Jim_Nvp * p, Jim_Obj *o, Jim_Nvp ** result)
{
    return Jim_Nvp_name2value(interp, p, Jim_String(o), result);
}


int Jim_Nvp_name2value(Jim_Interp *interp, const Jim_Nvp * _p, const char *name, Jim_Nvp ** result)
{
    const Jim_Nvp *p;

    p = Jim_Nvp_name2value_simple(_p, name);

    /* result */
    if (result) {
        *result = (Jim_Nvp *) (p);
    }

    /* found? */
    if (p->name) {
        return JIM_OK;
    }
    else {
        return JIM_ERR;
    }
}

int
Jim_Nvp_name2value_obj_nocase(Jim_Interp *interp, const Jim_Nvp * p, Jim_Obj *o, Jim_Nvp ** puthere)
{
    return Jim_Nvp_name2value_nocase(interp, p, Jim_String(o), puthere);
}

int
Jim_Nvp_name2value_nocase(Jim_Interp *interp, const Jim_Nvp * _p, const char *name,
    Jim_Nvp ** puthere)
{
    const Jim_Nvp *p;

    p = Jim_Nvp_name2value_nocase_simple(_p, name);

    if (puthere) {
        *puthere = (Jim_Nvp *) (p);
    }
    /* found */
    if (p->name) {
        return JIM_OK;
    }
    else {
        return JIM_ERR;
    }
}


int Jim_Nvp_value2name_obj(Jim_Interp *interp, const Jim_Nvp * p, Jim_Obj *o, Jim_Nvp ** result)
{
    int e;;
    jim_wide w;

    e = Jim_GetWide(interp, o, &w);
    if (e != JIM_OK) {
        return e;
    }

    return Jim_Nvp_value2name(interp, p, w, result);
}

Jim_Nvp *Jim_Nvp_value2name_simple(const Jim_Nvp * p, int value)
{
    while (p->name) {
        if (value == p->value) {
            break;
        }
        p++;
    }
    return ((Jim_Nvp *) (p));
}


int Jim_Nvp_value2name(Jim_Interp *interp, const Jim_Nvp * _p, int value, Jim_Nvp ** result)
{
    const Jim_Nvp *p;

    p = Jim_Nvp_value2name_simple(_p, value);

    if (result) {
        *result = (Jim_Nvp *) (p);
    }

    if (p->name) {
        return JIM_OK;
    }
    else {
        return JIM_ERR;
    }
}


int Jim_GetOpt_Setup(Jim_GetOptInfo * p, Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    memset(p, 0, sizeof(*p));
    p->interp = interp;
    p->argc = argc;
    p->argv = argv;

    return JIM_OK;
}

void Jim_GetOpt_Debug(Jim_GetOptInfo * p)
{
    int x;

    fprintf(stderr, "---args---\n");
    for (x = 0; x < p->argc; x++) {
        fprintf(stderr, "%2d) %s\n", x, Jim_String(p->argv[x]));
    }
    fprintf(stderr, "-------\n");
}


int Jim_GetOpt_Obj(Jim_GetOptInfo * goi, Jim_Obj **puthere)
{
    Jim_Obj *o;

    o = NULL;                   // failure
    if (goi->argc) {
        // success
        o = goi->argv[0];
        goi->argc -= 1;
        goi->argv += 1;
    }
    if (puthere) {
        *puthere = o;
    }
    if (o != NULL) {
        return JIM_OK;
    }
    else {
        return JIM_ERR;
    }
}

int Jim_GetOpt_String(Jim_GetOptInfo * goi, char **puthere, int *len)
{
    int r;
    Jim_Obj *o;
    const char *cp;


    r = Jim_GetOpt_Obj(goi, &o);
    if (r == JIM_OK) {
        cp = Jim_GetString(o, len);
        if (puthere) {
            /* remove const */
            *puthere = (char *)(cp);
        }
    }
    return r;
}

int Jim_GetOpt_Double(Jim_GetOptInfo * goi, double *puthere)
{
    int r;
    Jim_Obj *o;
    double _safe;

    if (puthere == NULL) {
        puthere = &_safe;
    }

    r = Jim_GetOpt_Obj(goi, &o);
    if (r == JIM_OK) {
        r = Jim_GetDouble(goi->interp, o, puthere);
        if (r != JIM_OK) {
            Jim_SetResultFormatted(goi->interp, "not a number: %#s", o);
        }
    }
    return r;
}

int Jim_GetOpt_Wide(Jim_GetOptInfo * goi, jim_wide * puthere)
{
    int r;
    Jim_Obj *o;
    jim_wide _safe;

    if (puthere == NULL) {
        puthere = &_safe;
    }

    r = Jim_GetOpt_Obj(goi, &o);
    if (r == JIM_OK) {
        r = Jim_GetWide(goi->interp, o, puthere);
    }
    return r;
}

int Jim_GetOpt_Nvp(Jim_GetOptInfo * goi, const Jim_Nvp * nvp, Jim_Nvp ** puthere)
{
    Jim_Nvp *_safe;
    Jim_Obj *o;
    int e;

    if (puthere == NULL) {
        puthere = &_safe;
    }

    e = Jim_GetOpt_Obj(goi, &o);
    if (e == JIM_OK) {
        e = Jim_Nvp_name2value_obj(goi->interp, nvp, o, puthere);
    }

    return e;
}

void Jim_GetOpt_NvpUnknown(Jim_GetOptInfo * goi, const Jim_Nvp * nvptable, int hadprefix)
{
    if (hadprefix) {
        Jim_SetResult_NvpUnknown(goi->interp, goi->argv[-2], goi->argv[-1], nvptable);
    }
    else {
        Jim_SetResult_NvpUnknown(goi->interp, NULL, goi->argv[-1], nvptable);
    }
}


int Jim_GetOpt_Enum(Jim_GetOptInfo * goi, const char *const *lookup, int *puthere)
{
    int _safe;
    Jim_Obj *o;
    int e;

    if (puthere == NULL) {
        puthere = &_safe;
    }
    e = Jim_GetOpt_Obj(goi, &o);
    if (e == JIM_OK) {
        e = Jim_GetEnum(goi->interp, o, lookup, puthere, "option", JIM_ERRMSG);
    }
    return e;
}

void
Jim_SetResult_NvpUnknown(Jim_Interp *interp,
    Jim_Obj *param_name, Jim_Obj *param_value, const Jim_Nvp * nvp)
{
    if (param_name) {
        Jim_SetResultFormatted(interp, "%#s: Unknown: %#s, try one of: ", param_name, param_value);
    }
    else {
        Jim_SetResultFormatted(interp, "Unknown param: %#s, try one of: ", param_value);
    }
    while (nvp->name) {
        const char *a;
        const char *b;

        if ((nvp + 1)->name) {
            a = nvp->name;
            b = ", ";
        }
        else {
            a = "or ";
            b = nvp->name;
        }
        Jim_AppendStrings(interp, Jim_GetResult(interp), a, b, NULL);
        nvp++;
    }
}

const char *Jim_Debug_ArgvString(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    static Jim_Obj *debug_string_obj;

    int x;

    if (debug_string_obj) {
        Jim_FreeObj(interp, debug_string_obj);
    }

    debug_string_obj = Jim_NewEmptyStringObj(interp);
    for (x = 0; x < argc; x++) {
        Jim_AppendStrings(interp, debug_string_obj, Jim_String(argv[x]), " ", NULL);
    }

    return Jim_String(debug_string_obj);
}

int Jim_nvpInit(Jim_Interp *interp)
{
    /* This is really a helper library, not an extension, but this is the easy way */
    return JIM_OK;
}
