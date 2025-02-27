#include <assert.h>
#include <stdint.h>
#include "io.h"

#define NO_IFACE -1

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

Node *node_alloc(void)
{
    Node *new = malloc(sizeof(Node));
    if (!new) {
        fprintf(stderr, "Buy more RAM lol\n");
        exit(1);
    }
    *new = (Node) { .out_iface = NO_IFACE };
    return new;
}

#define current_bit(node, ref) (((node).prefix >> (31 - (ref).prefix_length)) & 1)
void insert_node(Node *root, Node *new)
{
    uint32_t root_net_prefix = (root->prefix >> (31 - root->prefix_length)) & ((1U << root->prefix_length) - 1),
             new_net_prefix = (new->prefix >> (31 - new->prefix_length)) & ((1U << new->prefix_length) - 1);
    if (root->prefix_length == new->prefix_length &&
        root_net_prefix == new_net_prefix) {
        root->out_iface = new->out_iface;
        return;
    }

    if (current_bit(*new, *root)) {
        /* Reintentar inserción en rama derecha */
        if (!root->right) {
            root->right = node_alloc();
            root->right->prefix_length = root->prefix_length + 1;
            root->right->prefix = root->prefix | (1U << (31 - root->prefix_length));
        }
        insert_node(root->right, new);
    } else {
        /* Reintentar inserción en rama izquierda */
        if (!root->left) {
            root->left = node_alloc();
            root->left->prefix_length = root->prefix_length + 1;
            root->left->prefix = root->prefix;
        }
        insert_node(root->left, new);
    }
}

Node *create_trie()
{
    Node *root = node_alloc();
    int result = OK;
    //while (result != REACHED_EOF) {
    for (int i = 0; i < 2; ++i) { // Read only 1 for the moment
        uint32_t prefix = 0;
        int out_iface = 0;
        int pref_len = 0;
        result = readFIBLine(&prefix, &pref_len, &out_iface);
        if (result < 0) {
            printIOExplanationError(result);
            return root;
        }
        Node new_node = (Node) {
            .prefix = prefix,
            .prefix_length = pref_len,
            .out_iface = out_iface,
        };
        insert_node(root, &new_node);
    }
    return root;
}

void print_trie(FILE *stream, Node *root, int level)
{
    for (int i = 0; i < level; ++i)
        fputs("  ", stream);
    fprintf(stream, Node_Fmt "    Iface: %d\n", Node_Args(*root), root->out_iface);
    if (root->left) print_trie(stream, root->left, level + 1);
    if (root->right) print_trie(stream, root->right, level + 1);
}

void free_nodes(Node *root)
{
    if (root->left) free_nodes(root->left);
    if (root->right) free_nodes(root->right);
    free(root);
    root = NULL;
}

#define INPUT_FILE_PATH "prueba1.txt"
#define ROUTING_FILE_PATH "routing_table.txt"
int main(void)
{
    int return_value = 0;
    char routing_file_path[] = ROUTING_FILE_PATH;
    char input_file[] = INPUT_FILE_PATH;
    int result = initializeIO(routing_file_path, input_file);
    if (result < 0) {
        printIOExplanationError(result);
        return 1;
    }
    Node *root = create_trie();
    if (!root) {
        return_value = 1;
        goto out;
    }

    print_trie(stdout, root, 0);

out:
    free_nodes(root);
    freeIO();
    return return_value;
}
