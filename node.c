#include "node.h"
#include "io.h"
#include "utils.h"

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
    node_t left;
    node_t right;
};
/* Macros for printf */
#define Node_Fmt "%d.%d.%d.%d/%d"
#define Node_Args(x) (x).cidr_format[3], (x).cidr_format[2], (x).cidr_format[1], (x).cidr_format[0], (x).prefix_length


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
#define DEFAULT_CAPACITY 1024
typedef struct Node_Block Node_Block;
struct Node_Block {
    Node_Chunk *alloc;
    Node_Chunk *chunks;
    size_t count;
};

static Node_Block main_block = {0};

int node_count = 0;

static inline Node *idx2ptr(node_t node)
{
    if (node == -1) return NULL;
    return (Node *) &main_block.chunks[node];
}
static inline node_t ptr2idx(Node *node)
{
    if (!node) return -1;
    return (node_t) (node - (Node *) main_block.chunks);
}

int init_block(void)
{
    main_block.chunks = malloc(sizeof(Node_Chunk) * DEFAULT_CAPACITY);
    if (!main_block.chunks) {
        perror("malloc failed");
        return -1;
    }

    main_block.count = DEFAULT_CAPACITY;
    memset(main_block.chunks, 0, DEFAULT_CAPACITY);
    for (size_t i = 0; i < DEFAULT_CAPACITY - 1; ++i) {
        main_block.chunks[i].next = main_block.chunks + i + 1;
    }
    main_block.alloc = main_block.chunks;
    return 0;
}

void resize_block(void)
{
    /* Add 1 just in case it's 0 */
    size_t count = main_block.count;
    count += 1;
    count *= 2;
    main_block.chunks = realloc(main_block.chunks, count * sizeof(Node_Chunk));
    main_block.count = count;
}

/**********************************************************************
 * Allocate a node.
 * No args.
 **********************************************************************/
Node *node_alloc(void)
{
    if (!main_block.chunks) {
        if (init_block() < 0)
            return NULL;
    }

    if (!main_block.alloc) {
        size_t old_count = main_block.count;
        resize_block();
        for (size_t i = old_count - 1; i < main_block.count - 1; ++i) {
            main_block.chunks[i].next = main_block.chunks + i + 1;
        }
        main_block.alloc = main_block.chunks + old_count;
    }

    Node *new = (Node *) main_block.alloc;
    Node_Chunk *chunk = (Node_Chunk *) new;
    main_block.alloc = chunk->next;
    *new = (Node) { .out_iface = NO_IFACE, .left = -1, .right = -1 };
    return new;
}

/**********************************************************************
 * Free a node.
 **********************************************************************/
void node_free(Node *node) {
    Node_Chunk *chunk = (Node_Chunk *) node;
    chunk->next = (Node_Chunk *) main_block.alloc;
    main_block.alloc = chunk;
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
#define main_bit(node, ref) (((node).prefix >> (31 - (ref).prefix_length)) & 1)
#define main_bit_from_ip(ip, node) (((ip) >> (31 - (node).prefix_length)) & 1)
int insert_node(node_t root_idx, Node *new)
{
    Node *root = idx2ptr(root_idx);
    uint32_t root_net_prefix = (root->prefix >> (31 - root->prefix_length)) & ((1U << root->prefix_length) - 1),
             new_net_prefix = (new->prefix >> (31 - new->prefix_length)) & ((1U << new->prefix_length) - 1);

    /* Caso de encontrar la altura correcta */
    if (root->prefix_length == new->prefix_length &&
        root_net_prefix == new_net_prefix) {
        root->out_iface = new->out_iface;
        return 0;
    }

    /* Reintentar inserción en rama derecha */
    if (main_bit(*new, *root)) {
        /* Alojar derecho */
        if (root->right == -1) {
            Node *rhs = node_alloc();
            root = idx2ptr(root_idx);
            if (!rhs)
                return -1;
            rhs->prefix_length = root->prefix_length + 1;
            rhs->prefix = root->prefix | (1U << (31 - root->prefix_length));
            root->right = ptr2idx(rhs);
        }
        insert_node(root->right, new);
    /* Reintentar inserción en rama izquierda */
    } else {
        /* Alojar izquierdo */
        if (root->left == -1) {
            Node *lhs = node_alloc();
            root = idx2ptr(root_idx);
            if (!lhs)
                return -1;
            lhs->prefix_length = root->prefix_length + 1;
            lhs->prefix = root->prefix;
            root->left = ptr2idx(lhs);
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

        if (insert_node(ptr2idx(root), &new_node) < 0)
            break;
    }

    return ptr2idx(root);
}

/**********************************************************************
 * Free the tree from the root to the leaves.
 **********************************************************************/
void free_nodes(void)
{
    free(main_block.chunks);
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
    if (root->left) print_trie(stream, root->left, level + 1);
    if (root->right) print_trie(stream, root->right, level + 1);
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
        if (node->left != -1 && node->right == -1) {
            Node *child = idx2ptr(node->left);
            node_free(node);
            node_count += 1;
            return ptr2idx(child);
        }
        if (node->right != -1 && node->left == -1) {
            Node *child = idx2ptr(node->right);
            node_free(node);
            node_count += 1;
            return ptr2idx(child);
        }
    }

    return ptr2idx(node); // nodo con out_iface o 2 hijos
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
    Node *root = idx2ptr(root_idx);
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
    Node *lhs = idx2ptr(root->left);
    Node *rhs = idx2ptr(root->right);
    if (!level) {
        fprintf(stream, "digraph G {\n");
    }

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
    Node *root = idx2ptr(root_idx);
    FILE *stream = fopen(gv_file_path, "w");
    if (!stream) {
        fprintf(stderr, "ERROR: could not open GraphViz file\n");
        return -1;
    }
    make_graph(stream, root, 0);
    fclose(stream);  // We do not care about the errors at this point.
    return 0;
}
