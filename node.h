#ifndef NODE_H
#define NODE_H

#include <stdint.h>
#include <stdio.h>

#define NO_IFACE 0

extern int node_count;

/**********************************************************************
 * NODE STRUCTURE
 * Explanation of the anonymous union: convenience to print the IP
 *      addresses in CIDR format.
 * Fields:
 *  - prefix_length: the length of the IP prefix
 *  - prefix: the prefix itself (cidr_format represents the same bytes)
 *  - out_iface: the "next_hop" of the forwarding algorithm.
 *  - Node *left,*right: left and right subtrees.
 **********************************************************************/
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

/**********************************************************************
 * WARNING: THIS FUNCTION ALLOCATES MEMORY
 * Allocate a node.
 * No args.
 **********************************************************************/
Node *node_alloc(void);

/**********************************************************************
 * RECURSIVE FUNCTION
 * Insert a new node as the left/right subtree of another one
 * Args:
 *  - Node *root: the root of the current SUBTREE (it does not have to
 *  be the root of the whole tree, since this function is called
 *  recursively.
 *  - Node *new: the new node to be inserted in the tree.
 **********************************************************************/
void insert_node(Node *root, Node *new);

/**********************************************************************
 * WARNING: THIS FUNCTION ALLOCATES MEMORY
 * Create the Patricia trie, uncompressed.
 * Read the nodes one by one from the FIB, and put them on the tree.
 * No arguments.
 **********************************************************************/
Node *create_trie();

/**********************************************************************
 * Free the tree from the root to the leaves.
 **********************************************************************/
void free_nodes(Node *root);

/**********************************************************************
 * Compress a Patricia trie. Get rid of the in-between nodes if they
 * do not correspond to a next hop and they only have one subtree.
 **********************************************************************/
Node* compress_trie(Node *node);

/**********************************************************************
 * Look up the next hop corresponding to an IP. Returns 0 if it
 * did not find one.
 * Args:
 *  - Node *root: the absolute root of the trie.
 *  - uint32_t ip: the IP for which to look up a next hop.
 *  - int *accesses: variable declared outside the function to keep
 *  track of the number of memory acesses.
 **********************************************************************/
int lookup(Node *root, uint32_t ip, int *accesses);

/**********************************************************************
 * Wrapper of the above `make_graph` function, in charge of opening
 * the file and handling errors.
 * CALLS A RECURSIVE FUNCTION: make_graph
 * Output the trie to a file in graphviz format, to be processed with
 * the `dot` command-line utility
 * THIS FUNCTION PRODUCES LOGS. There is no need to print errors in
 * user code.
 * Args:
 *  - const char *gv_file_path: file path of the graphviz file path.
 *  - Node *root: the relative root of the current tree.
 **********************************************************************/
int output_graphviz(const char *gv_file_path, Node *root);

/**********************************************************************
 * Print the trie. OBSOLETE. We cannot see anything with this function
 **********************************************************************/
void print_trie(FILE *stream, Node *root, int level);
#endif // NODE_H
