#include "m61.hh"
#include <stdio.h>
#include <assert.h>
#include <string.h>
// Memory leak report with no leaks.

struct node {
    node* next;
};

int main() {
    node* list = nullptr;

    // create a list
    for (int i = 0; i < 400; ++i) {
        node* n = (node*) malloc(sizeof(node));
        n->next = list;
        list = n;
    }

    // free everything in it
    while (node* n = list) {
        list = n->next;
        free(n);
    }

    m61_printleakreport();
    printf("OK\n");
}

//! OK
