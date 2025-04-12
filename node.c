#include "node.h"
#include "io.h"
#include "utils.h"

/**********************************************************************
 * MEMORY ALLOCATION: Pool Allocator
 **********************************************************************/
typedef union Node_Chunk Node_Chunk;
union Node_Chunk {
    Node_Chunk *next;
    Node node;
};

/* Linked list of blocks */
/* Each block is an array of chunks with O(1) access time */
#define DEFAULT_CAPACITY 4096
typedef struct Node_Block Node_Block;
struct Node_Block {
    Node_Chunk *chunks;
};

#define BLOCKS_CAPACITY 1048576
typedef struct Block_Pool Block_Pool;
struct Block_Pool {
    size_t count;
    Node_Chunk *alloc;
    Node_Block blocks[BLOCKS_CAPACITY];
};

static Block_Pool main_pool = {0};

int node_count = 0;

int init_block(void)
{
    main_pool.blocks[main_pool.count].chunks = malloc(sizeof(Node_Chunk) * DEFAULT_CAPACITY);
    if (!main_pool.blocks[main_pool.count].chunks) {
        perror("malloc failed");
        return -1;
    }

    memset(main_pool.blocks[main_pool.count].chunks, 0, sizeof(Node_Chunk) * DEFAULT_CAPACITY);
    for (size_t j = 0; j < DEFAULT_CAPACITY - 1; ++j) {
        main_pool.blocks[main_pool.count].chunks[j].next = main_pool.blocks[main_pool.count].chunks + j + 1;
    }
    main_pool.alloc = main_pool.blocks[main_pool.count].chunks;
    return 0;
}

/**********************************************************************
 * Allocate a node.
 * No args.
 **********************************************************************/
Node *node_alloc(void)
{
    if (!main_pool.blocks[0].chunks) {
        if (init_block() < 0)
            return NULL;
    }

    if (!main_pool.alloc) {
        if (main_pool.count >= BLOCKS_CAPACITY - 1) {
            fprintf(stderr, "ERROR: out of memory!\n");
            return NULL;
        }
        main_pool.count += 1;
        if (init_block() < 0)
            return NULL;
    }

    Node *new = (Node *) main_pool.alloc;
    Node_Chunk *chunk = (Node_Chunk *) new;
    main_pool.alloc = chunk->next;
    *new = (Node) { .out_iface = NO_IFACE };
    return new;
}

/**********************************************************************
 * Free a node.
 **********************************************************************/
void node_free(Node *node) {
    Node_Chunk *chunk = (Node_Chunk *) node;
    chunk->next = (Node_Chunk *) main_pool.alloc;
    main_pool.alloc = chunk;
}

/**********************************************************************
 * RECURSIVE FUNCTION
 * Insert a new node as the left/right subtree of another one
 * Args:
 *  - Node *root: the root of the main SUBTREE (it does not have to
 *  be the root of the whole tree, since this function is called
 *  recursively.
 *  - Node *new: the new node to be inserted in the tree.
 **********************************************************************/
#define main_bit(node, ref) (((node).prefix >> (31 - (ref).prefix_length)) & 1U)
#define main_bit_from_ip(ip, node) (((ip) >> (31 - (node).prefix_length)) & 1U)
int insert_node(Node *root, Node *new)
{
    uint32_t root_net_prefix = (root->prefix >> (31 - root->prefix_length)) & ((1U << root->prefix_length) - 1);
    uint32_t new_net_prefix = (new->prefix >> (31 - new->prefix_length)) & ((1U << new->prefix_length) - 1);

    /* Caso de encontrar la altura correcta */
    if (root->prefix_length == new->prefix_length &&
        root_net_prefix == new_net_prefix) {
        root->out_iface = new->out_iface;
        return 0;
    }

    /* Reintentar inserción en rama derecha */
    if (main_bit(*new, *root)) {
        /* Alojar derecho */
        if (!root->right) {
            root->right = node_alloc();
            if (!root->right)
                return -1;
            root->right->prefix_length = root->prefix_length + 1;
            root->right->prefix = root->prefix | (1U << (31 - root->prefix_length));
        }
        insert_node(root->right, new);
        /* Reintentar inserción en rama izquierda */
    } else {
        /* Alojar izquierdo */
        if (!root->left) {
            root->left = node_alloc();
            if (!root->left)
                return -1;
            root->left->prefix_length = root->prefix_length + 1;
            root->left->prefix = root->prefix;
        }
        insert_node(root->left, new);
    }
    return 0;
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
    if (!root)
        return NULL;
    /* for (int i = 0; i < 10; ++i) { */
    for (;;) {
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

        if (insert_node(root, &new_node) < 0)
            break;
    }

    return root;
}

/**********************************************************************
 * Free the tree from the root to the leaves.
 **********************************************************************/
void free_nodes(void)
{
    for (size_t i = 0; i < BLOCKS_CAPACITY; ++i) {
        free(main_pool.blocks[i].chunks);
    }
}

/**********************************************************************
 * Print the trie. OBSOLETE. It's useless with big tries.
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
Node *compress_trie(Node *node) {
    if (!node) return NULL;

    node->left = compress_trie(node->left);
    node->right = compress_trie(node->right);

    if (!node->left && !node->right)
        return node;

    // Nodo sin interfaz y con un único hijo: eliminamos
    if (node->out_iface == NO_IFACE) {
        if (node->left && !node->right) {
            Node *child = node->left;
            node_free(node);
            node_count += 1;
            return child;
        }
        if (node->right&& !node->left) {
            Node *child = node->right;
            node_free(node);
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
            if (main_bit_from_ip(ip, *node) == 0) {
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
 *  - Node *root: the relative root of the main tree.
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
