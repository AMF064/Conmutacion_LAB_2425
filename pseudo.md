# Pseudocódigo
## Posible f. de lookup:
```
[[nullable]] lookup(node root, ip) :: prefix
{
    prefix result = nullptr;
    if (root.left) result = lookup(root.left, ip);
    if (((ip >> current_bit) & 1) && !result && root.right) result = lookup(root.right, ip);
    if (!((ip >> current_bit) & 1) && !result && root.left) result = lookup(root.left, ip);
    return result;
}
```

## Posible f. de creación del árbol:
- Formato del archivo de tabla de rutas:
```
PREFIX  OUT_IFACE
PREFIX  OUT_IFACE
PREFIX  OUT_IFACE
...
```

```
read_entire_file(FILE *file_path, node_pool *np) :: void
{
    initializeIO(file_path) error printExplanationError(?);
    int result = OK;
    while (result != REACHED_EOF) {
        uint32_t prefix;
        int pref_length, out_iface;
        result = readFIBLine(&prefix, &pref_len, &out_iface);
        node *root = node_new_childless(prefix, pref_len, out_iface);
        append(root, np);
    }
    freeIO();
}

insert_node(node *root, node *new) :: node *
{
    if (new.prefix_length < root.prefix_length) {
        /* Swap */
        node *tmp = root;
        root = new;
        new = tmp;
    }

    if (new.bit_that_matters == 1) // bit_that_matters será una máscara de bits, o un campo de bits en una estructura
        root.right = new;
    else
        root.left = new;
    return root;
}

create_trie(FILE *file_path) :: node * // TODO: incomplete. Gives us an incomplete trie.
{
    // We crossreference the nodes in the pool
    node_pool nodes = {0}; // The node pool registers the size
    read_entire_file(file_path, &nodes);
    node *root_candidate = nodes.items[0];
    for (auto i = 1; i < nodes.size; ++i)
        root_candidate = insert_node(root_candidate, nodes[i]);
}
```

# Profundidad del árbol

```
count_trie(node *root, size_t level = 0) :: size_t
{
    if (!root.left && !root.right)
        return level;
    size_t left_depth = count_trie(root.left, level + 1);
    size_t right_depth = count_trie(root.right, level + 1);
    return left_depth > right_depth ? left_depth : right_depth;
}
```


//Esto es una prueba
