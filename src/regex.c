/* Ordered Thompson NFA compiler and Pike VM. MIT License.
 * Algorithm background: https://swtch.com/~rsc/regexp/regexp2.html
 * Independent implementation; epsilon closure uses an explicit work stack.
 */
#include "cbat/regex.h"
#include <string.h>

enum { CBAT__RE_EMPTY, CBAT__RE_BYTE, CBAT__RE_ANY, CBAT__RE_CLASS,
    CBAT__RE_BEGIN, CBAT__RE_END, CBAT__RE_BOUNDARY, CBAT__RE_CONCAT,
    CBAT__RE_ALT, CBAT__RE_REPEAT, CBAT__RE_GROUP, CBAT__RE_SPLIT,
    CBAT__RE_SAVE, CBAT__RE_MATCH };
typedef struct cbat__re_node {
    int op, a, b, min, max, value;
    bool lazy, nullable;
    unsigned char bits[32];
} cbat__re_node;
typedef struct cbat__re_inst { int op, x, y, value; unsigned char bits[32]; } cbat__re_inst;
struct CBatRegex { CBatList *code; const CBatAllocator *allocator; int start, flags; size_t groups; };
typedef struct cbat__re_parser {
    const char *pattern; size_t at, length, groups; unsigned depth;
    CBatList *nodes; CBatRegex *regex; CBatError error; CBatParseInfo *info;
} cbat__re_parser;

static int cbat__re_fail(cbat__re_parser *p, CBatError error, const char *message) {
    if (p->error == CBAT_OK) {
        p->error = error;
        if (p->info != NULL) {
            size_t i;
            p->info->offset = p->at; p->info->line = 1; p->info->column = 1;
            for (i = 0; i < p->at; i++) {
                if (p->pattern[i] == '\n') { p->info->line++; p->info->column = 1; }
                else p->info->column++;
            }
            p->info->message = message;
        }
    }
    return -1;
}
static int cbat__re_node_add(cbat__re_parser *p, cbat__re_node node) {
    CBatError err;
    if (p->error != CBAT_OK) return -1;
    if (p->nodes->length >= 10000) return cbat__re_fail(p, CBAT_ERR_OUT_OF_RANGE, "pattern too large");
    if (node.op == CBAT__RE_EMPTY || node.op == CBAT__RE_BEGIN || node.op == CBAT__RE_END || node.op == CBAT__RE_BOUNDARY) node.nullable = true;
    else if (node.op == CBAT__RE_GROUP || node.op == CBAT__RE_REPEAT) {
        node.nullable = (node.op == CBAT__RE_REPEAT && node.min == 0) ||
            ((cbat__re_node *)cbat_list_get(p->nodes, (size_t)node.a))->nullable;
    } else if (node.op == CBAT__RE_CONCAT || node.op == CBAT__RE_ALT) {
        bool a = ((cbat__re_node *)cbat_list_get(p->nodes, (size_t)node.a))->nullable;
        bool b = ((cbat__re_node *)cbat_list_get(p->nodes, (size_t)node.b))->nullable;
        node.nullable = node.op == CBAT__RE_CONCAT ? a && b : a || b;
    }
    err = cbat_list_push(p->nodes, &node);
    if (err != CBAT_OK) return cbat__re_fail(p, err, "allocation failed");
    return (int)p->nodes->length - 1;
}
static int cbat__re_simple(cbat__re_parser *p, int op, int a, int b) {
    cbat__re_node n; memset(&n, 0, sizeof(n)); n.op = op; n.a = a; n.b = b;
    return cbat__re_node_add(p, n);
}
static void cbat__re_bit(unsigned char bits[32], unsigned c) { bits[c / 8] |= (unsigned char)(1u << (c % 8)); }
static bool cbat__re_has(const unsigned char bits[32], unsigned c) { return (bits[c / 8] & (1u << (c % 8))) != 0; }
static bool cbat__re_word(unsigned c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }
static unsigned cbat__re_fold(unsigned c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

/* One class atom or escaped literal. A singleton may be a range endpoint. */
static bool cbat__re_class_atom(cbat__re_parser *p, unsigned char bits[32], int *single) {
    unsigned c;
    bool invert = false;
    if (p->at == p->length) { cbat__re_fail(p, CBAT_ERR_PARSE, "missing character"); return false; }
    c = (unsigned char)p->pattern[p->at++];
    *single = (int)c;
    if (c == '\\') {
        unsigned i;
        if (p->at == p->length) { cbat__re_fail(p, CBAT_ERR_PARSE, "dangling escape"); return false; }
        c = (unsigned char)p->pattern[p->at++];
        if (c == 'D' || c == 'W' || c == 'S') { invert = true; c += 'a' - 'A'; }
        if (c == 'd' || c == 'w' || c == 's') {
            *single = -1;
            for (i = 0; i < 256; i++) {
                bool yes = c == 'd' ? i >= '0' && i <= '9' : c == 'w' ? cbat__re_word(i) :
                    i == ' ' || i == '\t' || i == '\r' || i == '\n' || i == '\v' || i == '\f';
                if (yes != invert) cbat__re_bit(bits, i);
            }
            return true;
        }
        if (c == 'n') c = '\n'; else if (c == 'r') c = '\r'; else if (c == 't') c = '\t';
        else if (c == 'f') c = '\f'; else if (c == 'v') c = '\v'; else if (c == 'b') c = '\b';
        else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            cbat__re_fail(p, CBAT_ERR_PARSE, "unsupported escape"); return false;
        }
        *single = (int)c;
    }
    cbat__re_bit(bits, c); return true;
}

static int cbat__re_expression(cbat__re_parser *p);
static int cbat__re_atom(cbat__re_parser *p) {
    cbat__re_node n;
    unsigned char c;
    memset(&n, 0, sizeof(n));
    if (p->at == p->length) return cbat__re_simple(p, CBAT__RE_EMPTY, 0, 0);
    c = (unsigned char)p->pattern[p->at++];
    if (c == '(') {
        int child, group = 0;
        if (++p->depth > 128) return cbat__re_fail(p, CBAT_ERR_OUT_OF_RANGE, "nesting too deep");
        if (p->at < p->length && p->pattern[p->at] == '?') {
            p->at++;
            if (p->at == p->length || p->pattern[p->at++] != ':') return cbat__re_fail(p, CBAT_ERR_PARSE, "unsupported group extension");
        } else {
            if (++p->groups >= CBAT_REGEX_MAX_GROUPS) return cbat__re_fail(p, CBAT_ERR_OUT_OF_RANGE, "too many captures");
            group = (int)p->groups;
        }
        child = cbat__re_expression(p);
        if (p->at == p->length || p->pattern[p->at++] != ')') return cbat__re_fail(p, CBAT_ERR_PARSE, "missing closing parenthesis");
        p->depth--;
        if (group == 0) return child;
        n.op = CBAT__RE_GROUP; n.a = child; n.value = group;
    } else if (c == '[') {
        bool negate = false, closed = false;
        n.op = CBAT__RE_CLASS;
        if (p->at < p->length && p->pattern[p->at] == '^') { negate = true; p->at++; }
        while (p->at < p->length && p->error == CBAT_OK) {
            unsigned char bits[32] = {0};
            int lo, hi;
            unsigned i;
            if (p->pattern[p->at] == ']') { p->at++; closed = true; break; }
            if (!cbat__re_class_atom(p, bits, &lo)) return -1;
            if (p->at + 1 < p->length && p->pattern[p->at] == '-' && p->pattern[p->at + 1] != ']') {
                unsigned char endbits[32] = {0}; p->at++;
                if (!cbat__re_class_atom(p, endbits, &hi)) return -1;
                if (lo < 0 || hi < lo) return cbat__re_fail(p, CBAT_ERR_PARSE, "invalid character range");
                for (i = (unsigned)lo; i <= (unsigned)hi; i++) cbat__re_bit(bits, i);
            }
            for (i = 0; i < 32; i++) n.bits[i] |= bits[i];
        }
        if (!closed) return cbat__re_fail(p, CBAT_ERR_PARSE, "missing closing bracket");
        if (p->regex->flags & CBAT_REGEX_IGNORE_CASE) {
            unsigned i;
            for (i = 'A'; i <= 'Z'; i++) if (cbat__re_has(n.bits, i) || cbat__re_has(n.bits, i + 32)) {
                cbat__re_bit(n.bits, i); cbat__re_bit(n.bits, i + 32);
            }
        }
        if (negate) { unsigned i; for (i = 0; i < 32; i++) n.bits[i] = (unsigned char)~n.bits[i]; }
    } else if (c == '.') n.op = CBAT__RE_ANY;
    else if (c == '^') n.op = CBAT__RE_BEGIN;
    else if (c == '$') n.op = CBAT__RE_END;
    else if (c == '\\' && p->at < p->length && p->pattern[p->at] == 'b') { p->at++; n.op = CBAT__RE_BOUNDARY; }
    else if (c == '*' || c == '+' || c == '?' || c == '{' || c == '}' || c == ']') return cbat__re_fail(p, CBAT_ERR_PARSE, "unexpected metacharacter");
    else {
        int single;
        p->at--; n.op = CBAT__RE_CLASS;
        if (!cbat__re_class_atom(p, n.bits, &single)) return -1;
        if (single >= 0) { n.op = CBAT__RE_BYTE; n.value = single; }
    }
    return cbat__re_node_add(p, n);
}

static int cbat__re_bound(cbat__re_parser *p) {
    int n = 0;
    if (p->at == p->length || p->pattern[p->at] < '0' || p->pattern[p->at] > '9') return cbat__re_fail(p, CBAT_ERR_PARSE, "expected repeat count");
    while (p->at < p->length && p->pattern[p->at] >= '0' && p->pattern[p->at] <= '9') {
        n = n * 10 + p->pattern[p->at++] - '0';
        if (n > 1000) return cbat__re_fail(p, CBAT_ERR_OUT_OF_RANGE, "repeat count too large");
    }
    return n;
}
static int cbat__re_piece(cbat__re_parser *p) {
    int atom = cbat__re_atom(p);
    cbat__re_node n;
    char c;
    if (p->error != CBAT_OK || p->at == p->length) return atom;
    c = p->pattern[p->at];
    if (c != '*' && c != '+' && c != '?' && c != '{') return atom;
    p->at++; memset(&n, 0, sizeof(n)); n.op = CBAT__RE_REPEAT; n.a = atom;
    n.min = c == '+' ? 1 : 0; n.max = c == '?' ? 1 : -1;
    if (c == '{') {
        n.min = cbat__re_bound(p); n.max = n.min;
        if (p->at < p->length && p->pattern[p->at] == ',') {
            p->at++; n.max = p->at < p->length && p->pattern[p->at] == '}' ? -1 : cbat__re_bound(p);
        }
        if (p->error != CBAT_OK) return -1;
        if (p->at == p->length || p->pattern[p->at++] != '}' || (n.max >= 0 && n.max < n.min)) return cbat__re_fail(p, CBAT_ERR_PARSE, "invalid repeat bounds");
    }
    if (p->at < p->length && p->pattern[p->at] == '?') { n.lazy = true; p->at++; }
    return cbat__re_node_add(p, n);
}

/* Balanced concatenation/alternation trees keep long flat patterns off the C stack. */
static int cbat__re_join(cbat__re_parser *p, CBatList *items, size_t lo, size_t hi, int op) {
    size_t middle;
    int left, right;
    if (lo == hi) return cbat__re_simple(p, CBAT__RE_EMPTY, 0, 0);
    if (hi - lo == 1) return *(int *)cbat_list_get(items, lo);
    middle = lo + (hi - lo)/2;
    left = cbat__re_join(p, items, lo, middle, op); right = cbat__re_join(p, items, middle, hi, op);
    return cbat__re_simple(p, op, left, right);
}
static int cbat__re_sequence(cbat__re_parser *p) {
    CBatList *items = cbat_list_new_with(sizeof(int), p->regex->allocator);
    int result = -1;
    if (items == NULL) return cbat__re_fail(p, CBAT_ERR_NO_MEMORY, "allocation failed");
    while (p->at < p->length && p->pattern[p->at] != ')' && p->pattern[p->at] != '|' && p->error == CBAT_OK) {
        int item = cbat__re_piece(p);
        CBatError err = p->error == CBAT_OK ? cbat_list_push(items, &item) : p->error;
        if (err != CBAT_OK) cbat__re_fail(p, err, "allocation failed");
    }
    if (p->error == CBAT_OK) result = cbat__re_join(p, items, 0, items->length, CBAT__RE_CONCAT);
    cbat_list_free(items); return result;
}
static int cbat__re_expression(cbat__re_parser *p) {
    CBatList *items = cbat_list_new_with(sizeof(int), p->regex->allocator);
    int result = -1;
    if (items == NULL) return cbat__re_fail(p, CBAT_ERR_NO_MEMORY, "allocation failed");
    do {
        int item = cbat__re_sequence(p);
        CBatError err = p->error == CBAT_OK ? cbat_list_push(items, &item) : p->error;
        if (err != CBAT_OK) cbat__re_fail(p, err, "allocation failed");
        if (p->at == p->length || p->pattern[p->at] != '|') break;
        p->at++;
    } while (p->error == CBAT_OK);
    if (p->error == CBAT_OK) result = cbat__re_join(p, items, 0, items->length, CBAT__RE_ALT);
    cbat_list_free(items); return result;
}

static int cbat__re_emit(cbat__re_parser *p, int op, int x, int y, int value, const unsigned char *bits) {
    cbat__re_inst inst;
    CBatError err;
    if (p->error != CBAT_OK) return -1;
    if (p->regex->code->length >= 10000) return cbat__re_fail(p, CBAT_ERR_OUT_OF_RANGE, "compiled program too large");
    memset(&inst, 0, sizeof(inst)); inst.op = op; inst.x = x; inst.y = y; inst.value = value;
    if (bits != NULL) memcpy(inst.bits, bits, 32);
    err = cbat_list_push(p->regex->code, &inst);
    if (err != CBAT_OK) return cbat__re_fail(p, err, "allocation failed");
    return (int)p->regex->code->length - 1;
}
static int cbat__re_compile_node(cbat__re_parser *p, int index, int next, unsigned depth) {
    cbat__re_node n;
    int a, b, i;
    if (p->error != CBAT_OK) return -1;
    if (depth > 256) return cbat__re_fail(p, CBAT_ERR_OUT_OF_RANGE, "compiled nesting too deep");
    n = *(cbat__re_node *)cbat_list_get(p->nodes, (size_t)index);
    switch (n.op) {
    case CBAT__RE_EMPTY: return next;
    case CBAT__RE_CONCAT:
        b = cbat__re_compile_node(p, n.b, next, depth + 1);
        return cbat__re_compile_node(p, n.a, b, depth + 1);
    case CBAT__RE_ALT:
        a = cbat__re_compile_node(p, n.a, next, depth + 1); b = cbat__re_compile_node(p, n.b, next, depth + 1);
        return cbat__re_emit(p, CBAT__RE_SPLIT, a, b, 0, NULL);
    case CBAT__RE_GROUP:
        b = cbat__re_emit(p, CBAT__RE_SAVE, next, 0, 2*n.value + 1, NULL);
        a = cbat__re_compile_node(p, n.a, b, depth + 1);
        return cbat__re_emit(p, CBAT__RE_SAVE, a, 0, 2*n.value, NULL);
    case CBAT__RE_REPEAT:
        if (n.max < 0) {
            b = cbat__re_emit(p, CBAT__RE_SPLIT, 0, 0, 0, NULL);
            a = cbat__re_compile_node(p, n.a, b, depth + 1);
            if (p->error != CBAT_OK) return -1;
            ((cbat__re_inst *)cbat_list_get(p->regex->code, (size_t)b))->x = n.lazy ? next : a;
            ((cbat__re_inst *)cbat_list_get(p->regex->code, (size_t)b))->y = n.lazy ? a : next;
            if (n.min == 0 && ((cbat__re_node *)cbat_list_get(p->nodes, (size_t)n.a))->nullable) {
                /* Nullable star needs a distinct entry split: (body+)? keeps
                 * the first empty iteration ahead of the zero-iteration exit. */
                next = cbat__re_emit(p, CBAT__RE_SPLIT, n.lazy ? next : a, n.lazy ? a : next, 0, NULL);
            } else if (n.min > 0) { next = a; n.min--; }
            else next = b;
        } else for (i = n.min; i < n.max && p->error == CBAT_OK; i++) {
            a = cbat__re_compile_node(p, n.a, next, depth + 1);
            next = cbat__re_emit(p, CBAT__RE_SPLIT, n.lazy ? next : a, n.lazy ? a : next, 0, NULL);
        }
        for (i = 0; i < n.min && p->error == CBAT_OK; i++) next = cbat__re_compile_node(p, n.a, next, depth + 1);
        return next;
    default: return cbat__re_emit(p, n.op, next, 0, n.value, n.bits);
    }
}

void cbat_regex_free(CBatRegex *regex) {
    if (regex != NULL) { cbat_list_free(regex->code); cbat_release(regex->allocator, regex); }
}
size_t cbat_regex_group_count(const CBatRegex *regex) { return regex == NULL ? 0 : regex->groups; }
CBatError cbat_regex_compile_with(const char *pattern, int flags, const CBatAllocator *allocator, CBatRegex **out, CBatParseInfo *info) {
    cbat__re_parser p;
    CBatRegex *regex;
    int root, end;
    if (pattern == NULL || out == NULL || (flags & ~7) != 0) return CBAT_ERR_INVALID_ARGUMENT;
    if (allocator == NULL) allocator = cbat_default_allocator();
    regex = (CBatRegex *)cbat_alloc_zeroed(allocator, sizeof(*regex));
    if (regex == NULL) return CBAT_ERR_NO_MEMORY;
    regex->allocator = allocator; regex->flags = flags;
    regex->code = cbat_list_new_with(sizeof(cbat__re_inst), allocator);
    memset(&p, 0, sizeof(p)); p.pattern = pattern; p.length = strlen(pattern); p.regex = regex; p.info = info;
    p.nodes = cbat_list_new_with(sizeof(cbat__re_node), allocator);
    if (p.nodes == NULL || regex->code == NULL) { cbat_list_free(p.nodes); cbat_regex_free(regex); return CBAT_ERR_NO_MEMORY; }
    root = cbat__re_expression(&p);
    if (p.error == CBAT_OK && p.at != p.length) cbat__re_fail(&p, CBAT_ERR_PARSE, "unexpected closing parenthesis");
    end = cbat__re_emit(&p, CBAT__RE_MATCH, 0, 0, 0, NULL);
    end = cbat__re_emit(&p, CBAT__RE_SAVE, end, 0, 1, NULL);
    root = cbat__re_compile_node(&p, root, end, 0);
    regex->start = cbat__re_emit(&p, CBAT__RE_SAVE, root, 0, 0, NULL); regex->groups = p.groups;
    cbat_list_free(p.nodes);
    if (p.error != CBAT_OK) { cbat_regex_free(regex); return p.error; }
    *out = regex; return CBAT_OK;
}
CBatError cbat_regex_compile(const char *pattern, int flags, CBatRegex **out, CBatParseInfo *info) {
    return cbat_regex_compile_with(pattern, flags, NULL, out, info);
}

typedef struct cbat__re_thread { int pc; size_t captures[2*CBAT_REGEX_MAX_GROUPS]; } cbat__re_thread;
typedef struct cbat__re_run {
    const CBatRegex *regex; const char *text; size_t length;
    cbat__re_thread *stack; unsigned char *seen;
} cbat__re_run;

/* Adds epsilon closure in priority order. Each instruction is visited once
 * per text position, across every incoming thread and newly injected start. */
static void cbat__re_closure(cbat__re_run *run, cbat__re_thread thread, size_t at,
    cbat__re_thread *list, size_t *count) {
    size_t pending = 0;
    run->stack[pending++] = thread;
    while (pending != 0) {
        const cbat__re_inst *inst;
        bool yes = true;
        thread = run->stack[--pending];
        if (run->seen[thread.pc]) continue;
        run->seen[thread.pc] = 1;
        inst = (const cbat__re_inst *)cbat_list_get(run->regex->code, (size_t)thread.pc);
        switch (inst->op) {
        case CBAT__RE_SPLIT:
            thread.pc = inst->y; run->stack[pending++] = thread;
            thread.pc = inst->x; run->stack[pending++] = thread;
            break;
        case CBAT__RE_SAVE:
            thread.captures[inst->value] = at; thread.pc = inst->x; run->stack[pending++] = thread;
            break;
        case CBAT__RE_BEGIN:
            yes = at == 0 || ((run->regex->flags & CBAT_REGEX_MULTILINE) && run->text[at-1] == '\n');
            if (yes) { thread.pc = inst->x; run->stack[pending++] = thread; }
            break;
        case CBAT__RE_END:
            yes = at == run->length || ((run->regex->flags & CBAT_REGEX_MULTILINE) && run->text[at] == '\n');
            if (yes) { thread.pc = inst->x; run->stack[pending++] = thread; }
            break;
        case CBAT__RE_BOUNDARY:
            yes = (at != 0 && cbat__re_word((unsigned char)run->text[at-1])) !=
                  (at != run->length && cbat__re_word((unsigned char)run->text[at]));
            if (yes) { thread.pc = inst->x; run->stack[pending++] = thread; }
            break;
        default: list[(*count)++] = thread; break;
        }
    }
}

static CBatError cbat__re_search(const CBatRegex *regex, const char *text, size_t length,
    size_t start, bool whole, CBatRegexMatch *out) {
    cbat__re_run run;
    cbat__re_thread *storage, *current, *next, candidate, initial;
    size_t bytes, count = 0, at, i, instructions = regex->code->length;
    bool found = false;
    if (!cbat_size_mul(instructions, 4*sizeof(cbat__re_thread), &bytes)) return CBAT_ERR_OVERFLOW;
    storage = (cbat__re_thread *)cbat_alloc(regex->allocator, bytes);
    run.seen = (unsigned char *)cbat_alloc_zeroed(regex->allocator, instructions);
    if (storage == NULL || run.seen == NULL) {
        cbat_release(regex->allocator, storage); cbat_release(regex->allocator, run.seen); return CBAT_ERR_NO_MEMORY;
    }
    run.regex = regex; run.text = text; run.length = length;
    current = storage; next = storage + instructions; run.stack = storage + 2*instructions;
    initial.pc = regex->start;
    for (i = 0; i < 2*CBAT_REGEX_MAX_GROUPS; i++) initial.captures[i] = CBAT_NPOS;
    for (at = start; at <= length; at++) {
        size_t next_count = 0;
        if (!found && (!whole || at == 0)) cbat__re_closure(&run, initial, at, current, &count);
        memset(run.seen, 0, instructions);
        for (i = 0; i < count; i++) {
            cbat__re_thread thread = current[i];
            const cbat__re_inst *inst = (const cbat__re_inst *)cbat_list_get(regex->code, (size_t)thread.pc);
            bool matches = false;
            if (inst->op == CBAT__RE_MATCH) {
                if (!whole || at == length) { candidate = thread; found = true; break; }
                continue;
            }
            if (at == length) continue;
            if (inst->op == CBAT__RE_BYTE) {
                unsigned a = (unsigned char)text[at], b = (unsigned)inst->value;
                matches = regex->flags & CBAT_REGEX_IGNORE_CASE ? cbat__re_fold(a) == cbat__re_fold(b) : a == b;
            } else if (inst->op == CBAT__RE_CLASS) matches = cbat__re_has(inst->bits, (unsigned char)text[at]);
            else if (inst->op == CBAT__RE_ANY) matches = text[at] != '\n' || (regex->flags & CBAT_REGEX_DOT_ALL);
            if (matches) { thread.pc = inst->x; cbat__re_closure(&run, thread, at + 1, next, &next_count); }
        }
        if ((found && next_count == 0) || at == length) break;
        { cbat__re_thread *swap = current; current = next; next = swap; count = next_count; }
    }
    if (found) {
        out->group_count = regex->groups + 1;
        for (i = 0; i < CBAT_REGEX_MAX_GROUPS; i++) {
            out->groups[i].start = candidate.captures[2*i]; out->groups[i].end = candidate.captures[2*i+1];
        }
    }
    cbat_release(regex->allocator, run.seen); cbat_release(regex->allocator, storage);
    return found ? CBAT_OK : CBAT_ERR_NOT_FOUND;
}

CBatError cbat_regex_search(const CBatRegex *regex, const char *text, size_t start, CBatRegexMatch *out) {
    size_t length;
    if (regex == NULL || text == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    length = strlen(text);
    if (start > length) return CBAT_ERR_OUT_OF_RANGE;
    return cbat__re_search(regex, text, length, start, false, out);
}
bool cbat_regex_is_match(const CBatRegex *regex, const char *text) {
    CBatRegexMatch match;
    return regex != NULL && text != NULL && cbat__re_search(regex, text, strlen(text), 0, true, &match) == CBAT_OK;
}

CBatError cbat_regex_find_all(const CBatRegex *regex, const char *text, CBatList **out) {
    CBatList *matches;
    CBatError err = CBAT_OK;
    size_t start = 0, length;
    if (regex == NULL || text == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    length = strlen(text); matches = cbat_list_new_with(sizeof(CBatRegexMatch), regex->allocator);
    if (matches == NULL) return CBAT_ERR_NO_MEMORY;
    while (start <= length) {
        CBatRegexMatch match;
        err = cbat__re_search(regex, text, length, start, false, &match);
        if (err == CBAT_ERR_NOT_FOUND) { err = CBAT_OK; break; }
        if (err != CBAT_OK) break;
        err = cbat_list_push(matches, &match);
        if (err != CBAT_OK) break;
        start = match.groups[0].end;
        if (start == match.groups[0].start) { if (start == length) break; start++; }
    }
    if (err != CBAT_OK) { cbat_list_free(matches); return err; }
    *out = matches; return CBAT_OK;
}

static CBatError cbat__re_append_piece(CBatList *pieces, const char *text, size_t length, const CBatAllocator *allocator) {
    CBatString *piece = cbat_string_new_with(allocator);
    CBatError err;
    if (piece == NULL) return CBAT_ERR_NO_MEMORY;
    err = cbat_string_append_length(piece, text, length);
    if (err == CBAT_OK) err = cbat_list_push(pieces, &piece);
    if (err != CBAT_OK) cbat_string_free(piece);
    return err;
}

CBatError cbat_regex_split(const CBatRegex *regex, const char *text, CBatList **out) {
    CBatList *pieces;
    CBatError err = CBAT_OK;
    size_t start = 0, copied = 0, length;
    if (regex == NULL || text == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    length = strlen(text); pieces = cbat_list_new_with(sizeof(CBatString *), regex->allocator);
    if (pieces == NULL) return CBAT_ERR_NO_MEMORY;
    cbat_list_set_destructor(pieces, cbat_string_destructor);
    while (start <= length) {
        CBatRegexMatch match;
        err = cbat__re_search(regex, text, length, start, false, &match);
        if (err == CBAT_ERR_NOT_FOUND) { err = CBAT_OK; break; }
        if (err != CBAT_OK) break;
        err = cbat__re_append_piece(pieces, text + copied, match.groups[0].start - copied, regex->allocator);
        if (err != CBAT_OK) break;
        copied = start = match.groups[0].end;
        if (start == match.groups[0].start) { if (start == length) break; start++; }
    }
    if (err == CBAT_OK) err = cbat__re_append_piece(pieces, text + copied, length - copied, regex->allocator);
    if (err != CBAT_OK) { cbat_list_free(pieces); return err; }
    *out = pieces; return CBAT_OK;
}

CBatError cbat_regex_replace(const CBatRegex *regex, const char *text, const char *replacement, CBatString **out) {
    CBatString *result;
    CBatError err = CBAT_OK;
    size_t start = 0, copied = 0, length, i;
    if (regex == NULL || text == NULL || replacement == NULL || out == NULL) return CBAT_ERR_INVALID_ARGUMENT;
    for (i = 0; replacement[i] != 0; i++) if (replacement[i] == '\\') {
        unsigned char c = (unsigned char)replacement[++i];
        if (c != '\\' && (c < '0' || c > '9' || (size_t)(c - '0') > regex->groups)) return CBAT_ERR_INVALID_ARGUMENT;
    }
    length = strlen(text); result = cbat_string_new_with(regex->allocator);
    if (result == NULL) return CBAT_ERR_NO_MEMORY;
    while (start <= length) {
        CBatRegexMatch match;
        err = cbat__re_search(regex, text, length, start, false, &match);
        if (err == CBAT_ERR_NOT_FOUND) { err = CBAT_OK; break; }
        if (err != CBAT_OK) break;
        err = cbat_string_append_length(result, text + copied, match.groups[0].start - copied);
        for (i = 0; replacement[i] != 0 && err == CBAT_OK; i++) {
            char c = replacement[i];
            if (c == '\\') {
                c = replacement[++i];
                if (c != '\\') {
                    CBatSpan span = match.groups[c - '0'];
                    if (span.start != CBAT_NPOS) err = cbat_string_append_length(result, text + span.start, span.end - span.start);
                    continue;
                }
            }
            err = cbat_string_append_char(result, c);
        }
        if (err != CBAT_OK) break;
        copied = start = match.groups[0].end;
        if (start == match.groups[0].start) { if (start == length) break; start++; }
    }
    if (err == CBAT_OK) err = cbat_string_append_length(result, text + copied, length - copied);
    if (err != CBAT_OK) { cbat_string_free(result); return err; }
    *out = result; return CBAT_OK;
}
