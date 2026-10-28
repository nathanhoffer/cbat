/* Recipe 14: A struct -> JSON -> text -> JSON -> struct round trip.
 * PROBLEM: Persist ordinary C data while validating types on input.
 * SOLUTION: Build a JSON object, serialize it, then use strict getters.
 * DISCUSSION: Build outputs privately and publish only after every operation
 * succeeds. String fields are copied: the resulting struct owns no JSON data.
 */
#include "cbat/cbat.h"
#include <stdio.h>
#include <string.h>

typedef struct Person { char name[64]; int64_t age; bool active; } Person;

static CBatError person_to_json(const Person *person, CBatJson **out) {
    CBatJson *object = cbat_json_new_object();
    CBatError err;
    if (object == NULL) return CBAT_ERR_NO_MEMORY;
    err = cbat_json_object_set_string(object, "name", person->name);
    if (err == CBAT_OK) err = cbat_json_object_set_int(object, "age", person->age);
    if (err == CBAT_OK) err = cbat_json_object_set_bool(object, "active", person->active);
    if (err != CBAT_OK) { cbat_json_free(object); return err; }
    *out = object; return CBAT_OK;
}

static CBatError person_from_json(const CBatJson *object, Person *out) {
    Person result;
    const char *name;
    size_t length;
    CBatError err = cbat_json_get_string(cbat_json_object_get(object, "name"), &name, &length);
    if (err != CBAT_OK) return err;
    if (length >= sizeof(result.name) || memchr(name, 0, length) != NULL) return CBAT_ERR_OUT_OF_RANGE;
    memcpy(result.name, name, length); result.name[length] = 0;
    err = cbat_json_get_int64(cbat_json_object_get(object, "age"), &result.age);
    if (err == CBAT_OK) err = cbat_json_get_bool(cbat_json_object_get(object, "active"), &result.active);
    if (err != CBAT_OK) return err;
    if (result.age < 0) return CBAT_ERR_OUT_OF_RANGE;
    *out = result; return CBAT_OK;
}

int main(void) {
    Person original = {"Ada", 36, true}, restored;
    CBatJson *object = NULL, *parsed = NULL;
    CBatString *text = NULL;
    CBatError err = person_to_json(&original, &object);
    if (err != CBAT_OK) goto cleanup;
    err = cbat_json_to_string(object, 2, &text);
    if (err != CBAT_OK) goto cleanup;
    err = cbat_json_parse(text->data, text->length, &parsed, NULL);
    if (err != CBAT_OK) goto cleanup;
    err = person_from_json(parsed, &restored);
    if (err == CBAT_OK) printf("%s\nRestored: %s, age %lld, active %s\n", text->data,
        restored.name, (long long)restored.age, restored.active ? "yes" : "no");
cleanup:
    if (err != CBAT_OK) fprintf(stderr, "%s\n", cbat_error_str(err));
    cbat_string_free(text); cbat_json_free(parsed); cbat_json_free(object);
    return err == CBAT_OK ? 0 : 1;
}
