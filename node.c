#include "node.h"
#include "io.h"
#include "utils.h"

int node_count = 0;

/**********************************************************************
 * Allocate a node.
 * No args.
 **********************************************************************/
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

/**********************************************************************
 * RECURSIVE FUNCTION
 * Insert a new node as the left/right subtree of another one
 * Args:
 *  - Node *root: the root of the current SUBTREE (it does not have to
 *  be the root of the whole tree, since this function is called
 *  recursively.
 *  - Node *new: the new node to be inserted in the tree.
 **********************************************************************/
#define current_bit(node, ref) (((node).prefix >> (31 - (ref).prefix_length)) & 1)
#define current_bit_from_ip(ip, node) (((ip) >> (31 - (node).prefix_length)) & 1)
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

/**********************************************************************
 * WARNING: THIS FUNCTION ALLOCATES MEMORY
 * Create the Patricia trie, uncompressed.
 * Read the nodes one by one from the FIB, and put them on the tree.
 * No arguments.
 **********************************************************************/
Node *create_trie()
{
    Node *root = node_alloc();
    for (;;) {
    //for (int i = 0; i < 10; ++i) {
        uint32_t prefix = 0;
        int out_iface = 0;
        int pref_len = 0;
        int result = readFIBLine(&prefix, &pref_len, &out_iface);
        if (result == REACHED_EOF)
            break;
        else if (result < 0) {
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

/**********************************************************************
 * Free the tree from the root to the leaves.
 **********************************************************************/
void free_nodes(Node *root)
{
    if (root->left) free_nodes(root->left);
    if (root->right) free_nodes(root->right);
    free(root);
    root = NULL;
}

/**********************************************************************
 * Print the trie. OBSOLETE. We cannot see anything with this function
 **********************************************************************/
void print_trie(FILE *stream, Node *root, int level)
{
    fprintf(stream, "(%d) ", level);
    for (int i = 0; i < level; ++i)
        fputs("  ", stream);
    fprintf(stream, Node_Fmt "    Iface: %d\n", Node_Args(*root), root->out_iface);
    if (root->left) print_trie(stream, root->left, level + 1);
    if (root->right) print_trie(stream, root->right, level + 1);
}

/**********************************************************************
 * Compress a Patricia trie. Get rid of the in-between nodes if they
 * do not correspond to a next hop and they only have one subtree.
 **********************************************************************/
Node* compress_trie(Node *node) {
    if (!node) return NULL;

    node->left = compress_trie(node->left);
    node->right = compress_trie(node->right);

    if (!node->left && !node->right)
        return node;

    // Nodo sin interfaz y con un único hijo: eliminamos
    if (node->out_iface == NO_IFACE) {
        if (node->left && !node->right) {
            Node *child = node->left;
            free(node);
            node_count += 1;
            return child;
        }
        if (node->right && !node->left) {
            Node *child = node->right;
            free(node);
            node_count += 1;
            return child;
        }
    }

    return node; // nodo con out_iface o 2 hijos
}


/**********************************************************************
 * Look up the next hop corresponding to an IP. Returns 0 if it
 * did not find one.
 * Args:
 *  - Node *root: the absolute root of the trie.
 *  - uint32_t ip: the IP for which to look up a next hop.
 *  - int *accesses: variable declared outside the function to keep
 *  track of the number of memory acesses.
 **********************************************************************/
int lookup(Node *root, uint32_t ip, int *accesses) {
    int best_iface = NO_IFACE;
    Node *node = root;
    while (node && best_iface == NO_IFACE) {
        *accesses += 1;
        int mask;
        getNetmask(node->prefix_length, (int *)&mask); //utils

         if (node->prefix_length == 0) {
            mask = 0x00000000;  // Protege contra desplazamiento ilegal(necesario por funcion getNetmask)
        }

        // Verificamos si el prefijo del nodo coincide con la IP
        if ((ip & mask) == (node->prefix & mask)) {
            if (node->out_iface != NO_IFACE) {//tiene interfaz
                best_iface = node->out_iface;
            }
            //seguimos avanzando
            if (current_bit_from_ip(ip, *node) == 0) {
                node = node->left;
            } else {
                node = node->right;
            }
        } else {
            //prefijo no coincide...
            break;
        }
    }
    return best_iface;
}

/**********************************************************************
 * RECURSIVE FUNCTION
 * Output the trie to a file in graphviz format, to be processed with
 * the `dot` command-line utility
 * Args:
 *  - FILE *stream: an opened FILE *. No need to handle it here.
 *  - Node *root: the relative root of the current tree.
 *  - int level: it is a closure. The function behaves differently
 *  depending on its value. If it is 0, it prints the opening and
 *  closing curly braces of the format.
 **********************************************************************/
void make_graph(FILE *stream, Node *root, int level)
{
    if (!level) {
        fprintf(stream, "digraph G {\n");
    }

    enum { BOTH, ONE, NONE } state = BOTH;

    if (!root->left && !root->right)
        state = NONE;
    else if ((root->left && !root->right) || (!root->left && root->right))
        state = ONE;
    else
        state = BOTH;

    if (state == BOTH)
        fprintf(stream, "\""Node_Fmt" * %d\" -> { ", Node_Args(*root), root->out_iface);
    else if (state == ONE)
        fprintf(stream, "\"" Node_Fmt " * %d\" -> ", Node_Args(*root), root->out_iface);
    else
        fprintf(stream, "\""Node_Fmt" * %d\"\n", Node_Args(*root), root->out_iface);

    if (root->left)
        fprintf(stream, "\""Node_Fmt" * %d\"", Node_Args(*root->left), root->left->out_iface);
    if (state == BOTH)
        fprintf(stream, ", ");
    if (root->right)
        fprintf(stream, "\""Node_Fmt" * %d\"", Node_Args(*root->right), root->right->out_iface);

    if (state == BOTH) fprintf(stream, "}\n");
    else fprintf(stream, "\n");

    if (root->left) make_graph(stream, root->left, level + 1);
    if (root->right) make_graph(stream, root->right, level + 1);

    if (!level) {
        fprintf(stream, "}\n");
    }
}

/**********************************************************************
 * Wrapper of the above `make_graph` function, in charge of opening
 * the file and handling errors.
 * THIS FUNCTION PRODUCES LOGS. There is no need to print errors in
 * user code.
 **********************************************************************/
int output_graphviz(const char *gv_file_path, Node *root)
{
    FILE *stream = fopen(gv_file_path, "w");
    if (!stream) {
        fprintf(stderr, "ERROR: could not open GraphViz file\n");
        return -1;
    }
    make_graph(stream, root, 0);
    fclose(stream);  // We do not care about the errors at this point.
    return 0;
}
