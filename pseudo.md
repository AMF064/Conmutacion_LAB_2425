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

insert_node(node *root, node *new) :: void  // Probablemente no necesite devolver node *
{
    // Caso final: el nodo es el mismo que la raíz: completa con la iface de salida
    if (root.prefix == new.prefix && root.prefix_length == new.prefix_length) {
        root.next_hop = new.next_hop;
        return;
    }

    if (new.bit_that_matters) {
        if (!root.right) root.right = context_alloc(appropriate_node);
        insert_node(root.right, new);
    } else {
        if (!root.left) root.left = context_alloc(appropriate_node);
        insert_node(root.left, new);
    }
}

create_trie(FILE *file_path) :: node *
{
    // We crossreference the nodes in the pool
    node_pool nodes = {0}; // La piscina de nodos tiene solo el scope de esta función.
    read_entire_file(file_path, &nodes);
    node *root = context_alloc(&(node) {0}); // La raíz va a ser 0.0.0.0/0. Alojada en otro contexto.
    for (auto i = 1; i < nodes.size; ++i)
        insert_node(root, nodes[i]);
}
```

# Profundidad del árbol

```
count_trie(node *root, size_t total = 0) :: size_t
{
    total += 1;
    if (root.left) total += count_trie(root.left);
    if (root.right) total += count_trie(root.right);
    return total;
}
```

# Pseudocódigo sin código 
1 Chequeamos el nº de argumentos de entrada
2 Creamos el Patricia Trie con create_trie(FIB)
3 Comprimimos el árbol con compress_trie
4 Contamos el nº de nodos con count_trie(root)
5 Realizamos el lookup(root,ip) a partir del InputPaketFile y creamos un archivo de salida con el nombre del 
archivo de entrada .out
En este archivo para cada IP ponemos su interfaz de salida (si tiene) , nº nodos hasta conseguirla (idea: campo de nodo que denote el nivel) y tiempo de procesado separado por ;
Mientras se va creando el archivo también se van actualizando unas vbles que muestran un report al final del archivo que contiene la siguiente info:
Number of nodes in the tree 
Packets processed 
Average node accesses
Average packet processing time (nsecs) 
Memory (Kbytes) 
CPU Time (secs) 
6 Fin del programa




