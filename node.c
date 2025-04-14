#include "node.h"
#include "io.h"
#include "utils.h"

#ifndef DEFAULT_CAPACITY
#define DEFAULT_CAPACITY 4096
#endif

#ifndef BLOCKS_CAPACITY
#define BLOCKS_CAPACITY 24
#endif

struct __node_t {
    uint32_t idx;
    uint32_t offset;
};

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
    uint8_t idx_offset;
    int prefix_length;
    union {
        uint32_t prefix;
        uint8_t cidr_format[4];
    };
    int out_iface;
    node_t left;
    node_t right;
};
/* Macros for printf */
#define Node_Fmt "%d.%d.%d.%d/%d"
#define Node_Args(x) (x).cidr_format[3], \
(x).cidr_format[2], \
(x).cidr_format[1], \
(x).cidr_format[0], \
(x).prefix_length

/**********************************************************************
 * MEMORY ALLOCATION: Pool Allocator
 **********************************************************************/

/**********************************************************************
 * Node chunks are a union between a pointer and a Node, so that they're
 * the size of the node.
 **********************************************************************/
typedef union Node_Chunk Node_Chunk;
union Node_Chunk {
    Node_Chunk *next;
    Node node;
};

/* Linked list of blocks */
/* Each block is an array of chunks with O(1) access time */
typedef struct Node_Block Node_Block;
struct Node_Block {
    Node_Chunk *chunks;
};

typedef struct Block_Pool Block_Pool;
struct Block_Pool {
    uint8_t count;
    Node_Chunk *alloc;
    Node_Block blocks[BLOCKS_CAPACITY];
};

static Block_Pool main_pool = {0};

int node_count = 0;

node_t ptr2idx(Node *node) {
    if (node == NULL)
        return -1;
    struct __node_t idx = { .idx = (uint32_t) (node - (Node *) main_pool.blocks[node->idx_offset].chunks), .offset = (uint32_t) node->idx_offset };
    /* Reinterpret the bits */
    node_t retval = *(node_t *) &idx;
    //node_t retval = ((node_t) idx.idx << 32) | (idx.offset);
    return retval;
}

Node *idx2ptr(node_t node) {
    if (node == -1)
        return NULL;
    struct __node_t hidden = *(struct __node_t *) &node;
    //struct __node_t hidden = { .idx = (node >> 32) & ((1ULL << 32) - 1), .offset = node & ((1ULL << 32) - 1) };
    return (Node *) &main_pool.blocks[hidden.offset].chunks[hidden.idx];
}

int init_block(void)
{
    main_pool.blocks[main_pool.count].chunks = malloc(sizeof(Node_Chunk) * DEFAULT_CAPACITY);
    if (!main_pool.blocks[main_pool.count].chunks) {
        perror("malloc failed");
        return -1;
    }

    memset(main_pool.blocks[main_pool.count].chunks, 0, sizeof(Node_Chunk) * DEFAULT_CAPACITY);
    /* Cross-reference the chunks, so that we can access the next one available in O(1) time */
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
    *new = (Node) { .out_iface = NO_IFACE, .left = -1, .right = -1, .idx_offset = main_pool.count };
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
#define main_bit(node, ref) (((node).prefix >> (31 - (ref).prefix_length)) & 1ULL)
#define main_bit_from_ip(ip, node) (((ip) >> (31 - (node).prefix_length)) & 1ULL)
int insert_node(Node *root, Node *new)
{
    //printf("Accessing node %p\nEquivalent index:  %ld\n", (void *) root, ptr2idx(root));

    uint32_t root_net_prefix = (root->prefix >> (31 - root->prefix_length)) & ((1ULL << root->prefix_length) - 1);
    uint32_t new_net_prefix = (new->prefix >> (31 - new->prefix_length)) & ((1ULL << new->prefix_length) - 1);

    /* Caso de encontrar la altura correcta */
    if (root->prefix_length == new->prefix_length &&
        root_net_prefix == new_net_prefix) {
        root->out_iface = new->out_iface;
        return 0;
    }

    /* Reintentar inserción en rama derecha */
    if (main_bit(*new, *root)) {
        /* Alojar derecho */
        Node *rhs = idx2ptr(root->right);
        if (!rhs) {
            rhs = node_alloc();
            if (!rhs)
                return -1;
            rhs->prefix_length = root->prefix_length + 1;
            rhs->prefix = root->prefix | (1ULL << (31 - root->prefix_length));
            root->right = ptr2idx(rhs);
        }
        insert_node(rhs, new);
        /* Reintentar inserción en rama izquierda */
    } else {
        /* Alojar izquierdo */
        Node *lhs = idx2ptr(root->left);
        if (!lhs) {
            lhs = node_alloc();
            if (!lhs)
                return -1;
            lhs->prefix_length = root->prefix_length + 1;
            lhs->prefix = root->prefix;
            root->left = ptr2idx(lhs);
        }
        insert_node(lhs, new);
    }
    return 0;
}

/**********************************************************************
 * WARNING: THIS FUNCTION ALLOCATES MEMORY
 * Create the Patricia trie, uncompressed.
 * Read the nodes one by one from the FIB, and put them on the tree.
 * No arguments.
 **********************************************************************/
node_t create_trie()
{
    Node *root = node_alloc();
    if (!root)
        return -1;
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
            return ptr2idx(root);
        }
        Node new_node = (Node) {
            .prefix = prefix,
            .prefix_length = pref_len,
            .out_iface = out_iface,
        };

        if (insert_node(root, &new_node) < 0)
            break;
    }

    return ptr2idx(root);
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
void print_trie(FILE *stream, node_t root_idx, int level)
{
    Node *root = idx2ptr(root_idx);
    fprintf(stream, "(%d) ", level);
    for (int i = 0; i < level; ++i)
        fputs("  ", stream);
    fprintf(stream, Node_Fmt "    Iface: %d\n", Node_Args(*root), root->out_iface);
    if (root->left != -1) print_trie(stream, root->left, level + 1);
    if (root->right != -1) print_trie(stream, root->right, level + 1);
}

/**********************************************************************
 * Compress a Patricia trie. Get rid of the in-between nodes if they
 * do not correspond to a next hop and they only have one subtree.
 **********************************************************************/
node_t compress_trie(node_t node_idx) {
    Node *node = idx2ptr(node_idx);
    if (!node) return -1;

    node->left = compress_trie(node->left);
    node->right = compress_trie(node->right);

    if (!node->left && !node->right)
        return ptr2idx(node);

    // Nodo sin interfaz y con un único hijo: eliminamos
    if (node->out_iface == NO_IFACE) {
        if (node->left && !node->right) {
            node_t child = node->left;
            node_free(node);
            node_count += 1;
            return child;
        }
        if (node->right&& !node->left) {
            node_t child = node->right;
            node_free(node);
            node_count += 1;
            return child;
        }
    }

    return node_idx; // nodo con out_iface o 2 hijos
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
int lookup(node_t root_idx, uint32_t ip, int *accesses) {
    int best_iface = NO_IFACE;
    Node *node = idx2ptr(root_idx);
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
                node = idx2ptr(node->left);
            } else {
                node = idx2ptr(node->right);
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

    Node *lhs = idx2ptr(root->left);
    Node *rhs = idx2ptr(root->right);

    enum { BOTH, ONE, NONE } state = BOTH;

    if (!lhs && !rhs)
        state = NONE;
    else if ((lhs && !rhs) || (!lhs && rhs))
        state = ONE;
    else
        state = BOTH;

    if (state == BOTH)
        fprintf(stream, "\""Node_Fmt" * %d\" -> { ", Node_Args(*root), root->out_iface);
    else if (state == ONE)
        fprintf(stream, "\"" Node_Fmt " * %d\" -> ", Node_Args(*root), root->out_iface);
    else
        fprintf(stream, "\""Node_Fmt" * %d\"\n", Node_Args(*root), root->out_iface);

    if (lhs)
        fprintf(stream, "\""Node_Fmt" * %d\"", Node_Args(*lhs), lhs->out_iface);
    if (state == BOTH)
        fprintf(stream, ", ");
    if (rhs)
        fprintf(stream, "\""Node_Fmt" * %d\"", Node_Args(*rhs), rhs->out_iface);

    if (state == BOTH) fprintf(stream, "}\n");
    else fprintf(stream, "\n");

    if (lhs) make_graph(stream, lhs, level + 1);
    if (rhs) make_graph(stream, rhs, level + 1);

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
int output_graphviz(const char *gv_file_path, node_t root_idx)
{
    FILE *stream = fopen(gv_file_path, "w");
    if (!stream) {
        fprintf(stderr, "ERROR: could not open GraphViz file\n");
        return -1;
    }
    Node *root = idx2ptr(root_idx);
    make_graph(stream, root, 0);
    fclose(stream);  // We do not care about the errors at this point.
    return 0;
}
