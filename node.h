#ifndef NODE_H
#define NODE_H

#include <stdint.h>
#include <stdio.h>

#define NO_IFACE 0

extern int node_count;

typedef int64_t node_t;

/**********************************************************************
 * WARNING: THIS FUNCTION ALLOCATES MEMORY
 * Create the Patricia trie, uncompressed.
 * Read the nodes one by one from the FIB, and put them on the tree.
 * No arguments.
 **********************************************************************/
node_t create_trie();

/**********************************************************************
 * Free the tree from the root to the leaves.
 **********************************************************************/
void free_nodes(void);

/**********************************************************************
 * Compress a Patricia trie. Get rid of the in-between nodes if they
 * do not correspond to a next hop and they only have one subtree.
 **********************************************************************/
node_t compress_trie(node_t node);

/**********************************************************************
 * Look up the next hop corresponding to an IP. Returns 0 if it
 * did not find one.
 * Args:
 *  - node_t root: the absolute root of the trie.
 *  - uint32_t ip: the IP for which to look up a next hop.
 *  - int *accesses: variable declared outside the function to keep
 *  track of the number of memory acesses.
 **********************************************************************/
int lookup(node_t root, uint32_t ip, int *accesses);

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
 *  - node_t root: the relative root of the current tree.
 **********************************************************************/
int output_graphviz(const char *gv_file_path, node_t root);

/**********************************************************************
 * Print the trie. OBSOLETE. We cannot see anything with this function
 **********************************************************************/
void print_trie(FILE *stream, node_t root, int level);
#endif // NODE_H
