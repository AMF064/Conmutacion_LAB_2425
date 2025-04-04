#ifndef NODE_H
#define NODE_H

#include <stdint.h>
#include <stdio.h>

#define NO_IFACE -1

extern int node_count;

typedef struct Node Node;
struct Node {
    int prefix_length;
    union {
        uint32_t prefix;
        uint8_t cidr_format[4];
    };
    int out_iface;
    Node *left;
    Node *right;
};
/* Macros for printf */
#define Node_Fmt "%d.%d.%d.%d/%d"
#define Node_Args(x) (x).cidr_format[3], (x).cidr_format[2], (x).cidr_format[1], (x).cidr_format[0], (x).prefix_length

Node *node_alloc(void);
void insert_node(Node *root, Node *new);
Node *create_trie();
void free_nodes(Node *root);
void make_graph(FILE *stream, Node *root, int level);
int output_graphviz(const char *gv_file_path, Node *root);
void print_trie(FILE *stream, Node *root, int level);
#endif // NODE_H
