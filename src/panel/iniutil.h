/*
 * plugin/src/panel/iniutil.h
 *
 * Created by Toby G on 12/07/2026.
 *
 * shared emhttp .ini / .notify parse helpers
 * Part of panel_dash: #included by panel_dash.c in a fixed order; not a
 * standalone translation unit (relies on the includes and earlier modules).
 */
#ifndef PANEL_INIUTIL_H
#define PANEL_INIUTIL_H

/* ---------- generic ini helpers (emhttp .ini / .notify files) ---------- */
static int ini_kv(char *line, char **k, char **v){
    char *eq = strchr(line, '='); if (!eq) return 0;
    *eq = 0; *k = line; *v = eq + 1;
    (*v)[strcspn(*v, "\r\n")] = 0;
    size_t n = strlen(*v);
    if (n >= 2 && (*v)[0] == '"' && (*v)[n-1] == '"'){
        (*v)[n-1] = 0; (*v)++;
        char *s = *v, *d = *v;                  /* unescape \" and \\ */
        while (*s){ if (*s == '\\' && (s[1] == '"' || s[1] == '\\')) s++; *d++ = *s++; }
        *d = 0;
    }
    return 1;
}
static int read_ull_file(const char *p, unsigned long long *out){
    FILE *f = fopen(p, "r"); if (!f) return 0;
    int ok = fscanf(f, "%llu", out) == 1; fclose(f); return ok;
}


#endif /* PANEL_INIUTIL_H */
