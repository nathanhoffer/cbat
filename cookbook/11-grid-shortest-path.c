/*
 * Recipe 11: Shortest path on a grid (breadth-first search)
 *
 * PROBLEM
 *   Find the shortest route through a maze - the classic puzzle a robot
 *   in a grid world needs to solve.
 *
 * SOLUTION
 *   BFS with a CBatDeque as the queue: push the start, pop from the front,
 *   push unvisited neighbours to the back. Remember each cell's parent to
 *   rebuild the path.
 *
 * DISCUSSION
 *   A deque gives O(1) pops from the front, which a plain list cannot.
 *   Because BFS explores in rings of equal distance, the first time it
 *   reaches the goal is along a shortest path.
 */
#include <stdio.h>
#include <string.h>

#include "cbat/cbat.h"

#define W 10
#define H 6

typedef struct Cell {
    int x;
    int y;
} Cell;

int main(void) {
    char maze[H][W + 1] = {
        "S.#.......",
        ".##.####..",
        "....#..#..",
        "#.###.##.#",
        "..#......#",
        "#...##.#.G",
    };
    int parent[H][W];
    CBatDeque *queue = cbat_deque_new(sizeof(Cell));
    Cell start = {0, 0};
    Cell goal = {9, 5};
    Cell current;
    bool found = false;
    int steps = 0;
    int y;
    static const int dx[4] = {1, -1, 0, 0};
    static const int dy[4] = {0, 0, 1, -1};

    if (queue == NULL) {
        return 1;
    }
    memset(parent, -1, sizeof(parent));
    parent[start.y][start.x] = start.y * W + start.x;
    cbat_deque_push_back(queue, &start);

    while (cbat_deque_pop_front(queue, &current) == CBAT_OK) {
        int d;
        if (current.x == goal.x && current.y == goal.y) {
            found = true;
            break;
        }
        for (d = 0; d < 4; d++) {
            Cell next;
            next.x = current.x + dx[d];
            next.y = current.y + dy[d];
            if (next.x < 0 || next.y < 0 || next.x >= W || next.y >= H ||
                maze[next.y][next.x] == '#' || parent[next.y][next.x] != -1) {
                continue;
            }
            parent[next.y][next.x] = current.y * W + current.x;
            cbat_deque_push_back(queue, &next);
        }
    }
    cbat_deque_free(queue);

    if (!found) {
        puts("no path");
        return 1;
    }
    /* Walk back from the goal, marking the path. */
    current = goal;
    while (!(current.x == start.x && current.y == start.y)) {
        int p = parent[current.y][current.x];
        if (maze[current.y][current.x] == '.') {
            maze[current.y][current.x] = '*';
        }
        current.x = p % W;
        current.y = p / W;
        steps++;
    }
    for (y = 0; y < H; y++) {
        puts(maze[y]);
    }
    printf("shortest path: %d steps\n", steps);
    return 0;
}
