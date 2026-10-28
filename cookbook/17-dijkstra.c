/*
 * Recipe 17: Shortest path with Dijkstra
 *
 * PROBLEM
 *   Find the cheapest route through a weighted grid, where the shortest
 *   route in number of steps may cost more than a longer route.
 *
 * SOLUTION
 *   Store {cost, cell} work items in a min-heap. Pop the cheapest candidate,
 *   relax its neighbors, and discard stale entries whose cost is outdated.
 *
 * DISCUSSION
 *   This is Dijkstra's algorithm: weights must be nonnegative. Heap entries
 *   are structs, so pass addresses of local variables rather than the
 *   scalar-only convenience macro. Both the comparator and the byte copies
 *   work with ordinary C structs; the heap owns its copies, not the locals.
 */
#include <limits.h>
#include <stdio.h>
#include "cbat/cbat.h"

typedef struct Work { int cost; int cell; } Work;
static int cheaper(const void *a, const void *b) {
    const Work *x = (const Work *)a, *y = (const Work *)b;
    return (x->cost > y->cost) - (x->cost < y->cost);
}

int main(void) {
    const int weights[9] = {1, 9, 1, 1, 9, 1, 1, 1, 1};
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    int distance[9], i;
    Work first = {0, 0};
    CBatError err;
    CBatHeap *heap = cbat_heap_new(sizeof(Work), cheaper);
    if (heap == NULL) return 1;
    for (i = 0; i < 9; i++) distance[i] = INT_MAX;
    distance[0] = 0;
    err = cbat_heap_push(heap, &first);
    while (err == CBAT_OK && !cbat_heap_is_empty(heap)) {
        Work current;
        err = cbat_heap_pop(heap, &current);
        if (err != CBAT_OK || current.cell == 8) break;
        if (current.cost != distance[current.cell]) continue;
        for (i = 0; i < 4; i++) {
            int x = current.cell % 3 + dx[i], y = current.cell / 3 + dy[i];
            int cell, cost;
            Work next;
            if (x < 0 || x >= 3 || y < 0 || y >= 3) continue;
            cell = y * 3 + x;
            cost = current.cost + weights[cell];
            if (cost >= distance[cell]) continue;
            distance[cell] = cost;
            next.cost = cost; next.cell = cell;
            err = cbat_heap_push(heap, &next);
            if (err != CBAT_OK) break;
        }
    }
    if (err == CBAT_OK) printf("Cheapest route from cell 0 to 8: %d\n", distance[8]);
    else fprintf(stderr, "Dijkstra: %s\n", cbat_error_str(err));
    cbat_heap_free(heap);
    return err == CBAT_OK && distance[8] == 4 ? 0 : 1;
}
