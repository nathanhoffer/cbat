/*
 * Recipe 3: Summarizing a CSV file
 *
 * PROBLEM
 *   Given sales records as CSV, compute the total and median sale per
 *   region and write the summary as a new CSV.
 *
 * SOLUTION
 *   Parse with cbat_csv_parse(), group amounts into a CBatDict whose values
 *   are CBatList pointers (one list of doubles per region), then use
 *   cbat_stats_* on each list's data and cbat_csv_append_row() to write.
 *
 * DISCUSSION
 *   Setting cbat_list_destructor on the dict means freeing the dict frees
 *   every per-region list too - one call cleans up the whole structure.
 *   Quoted fields such as "North, East" are handled by the parser.
 */
#include <stdio.h>
#include <string.h>

#include "cbat/cbat.h"

int main(void) {
    const char *csv =
        "region,product,amount\n"
        "\"North, East\",widget,120.50\n"
        "South,gadget,80\n"
        "\"North, East\",gadget,99.99\n"
        "South,widget,45.25\n"
        "West,widget,300\n"
        "South,gizmo,60\n";
    CBatList *rows = NULL;
    CBatDict *by_region = cbat_dict_new(sizeof(CBatList *));
    CBatString *report = cbat_string_new();
    CBatParseInfo info;
    size_t i;
    size_t it = 0;
    const char *region;
    void *slot;
    const char *header[] = {"region", "sales", "total", "median"};

    if (by_region == NULL || report == NULL) {
        return 1;
    }
    cbat_dict_set_destructor(by_region, cbat_list_destructor);

    if (cbat_csv_parse(csv, strlen(csv), NULL, &rows, &info) != CBAT_OK) {
        printf("bad CSV at line %lu: %s\n", (unsigned long)info.line, info.message);
        return 1;
    }

    for (i = 1; i < cbat_list_length(rows); i++) { /* row 0 is the header */
        CBatList *fields = CBAT_LIST_GET(rows, CBatList *, i);
        const char *name = cbat_string_cstr(CBAT_LIST_GET(fields, CBatString *, 0));
        double amount = 0;
        CBatList **amounts = (CBatList **)cbat_dict_get(by_region, name);
        if (cbat_string_to_double(cbat_string_cstr(CBAT_LIST_GET(fields, CBatString *, 2)), &amount) != CBAT_OK) {
            printf("skipping row %lu: bad amount\n", (unsigned long)i);
            continue;
        }
        if (amounts == NULL) {
            CBatList *fresh = cbat_list_new(sizeof(double));
            cbat_dict_set(by_region, name, &fresh);
            amounts = (CBatList **)cbat_dict_get(by_region, name);
        }
        CBAT_LIST_PUSH(*amounts, double, amount);
    }

    cbat_csv_append_row(report, header, 4, NULL);
    while (cbat_dict_next(by_region, &it, &region, &slot)) {
        CBatList *amounts = *(CBatList **)slot;
        const double *data = (const double *)amounts->data;
        double median = 0;
        char count_text[32];
        char total_text[32];
        char median_text[32];
        const char *row[4];
        cbat_stats_median(data, amounts->length, &median);
        snprintf(count_text, sizeof(count_text), "%lu", (unsigned long)amounts->length);
        snprintf(total_text, sizeof(total_text), "%.2f", cbat_stats_sum(data, amounts->length));
        snprintf(median_text, sizeof(median_text), "%.2f", median);
        row[0] = region;
        row[1] = count_text;
        row[2] = total_text;
        row[3] = median_text;
        cbat_csv_append_row(report, row, 4, NULL);
    }

    fputs(cbat_string_cstr(report), stdout);
    cbat_string_free(report);
    cbat_dict_free(by_region);
    cbat_list_free(rows);
    return 0;
}
