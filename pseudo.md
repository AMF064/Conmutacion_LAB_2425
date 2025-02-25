# Pseudocódigo
## Posible f. de lookup:
```
[[nullable]] lookup(node root, ip) -> prefix
{
    prefix result = nullptr;
    if (root.left) result = lookup(root.left, ip);
    if (!result && root.right) result = lookup(root.right, ip);
    if (!result && root.is_prefix) result = root;
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
Nota: no está equiespaciado, pero da igual, ya tenemos cómo parsearlo.
```
read_entire_file(FILE *file_path, node_pool *np) -> void
{
    initializeIO(file_path)? && printExplanationError(?); // Futuristic error handling
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

create_trie(FILE *file_path) -> node *
{
    // We crossreference the nodes in the pool
    node_pool nodes = {0}; // The node pool registers the size
    read_entire_file(file_path, &nodes);
    for (auto i = 0; i < nodes.size; ++i) {
    }
}
```


//Esto es una prueba
