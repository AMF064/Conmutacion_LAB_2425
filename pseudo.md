# Pseudocódigo

## Por
- Sergio Cruz Expósito: 100496315
- Álvaro Masa Fernández: 100496239

## Elección del algoritmo
Nuestra elección inicial es el trie comprimido, pero estamos pensando en cambiar a trie LC en el futuro.

## Pasos
1. Chequeamos el nº de argumentos de entrada
2. Creamos el Patricia Trie con `create_trie`(FIB)
3. Comprimimos el árbol con `compress_trie`
4. Contamos el nº de nodos con `count_trie(root)`
5. Medimos tiempo con clock_gettime(CLOCK_MONOTONIC_RAW, &initialTime)
6. Realizamos el lookup(root,ip) a partir del InputPaketFile y creamos un archivo de salida con el nombre del
archivo de entrada .out
    - En este archivo para cada IP ponemos su interfaz de salida (si tiene) , nº nodos hasta conseguirla (idea: campo de nodo que denote el nivel) y tiempo de procesado separado por ;
    - Mientras se va creando el archivo también se van actualizando unas vbles que muestran un report al final del archivo que contiene la siguiente info:
        - Number of nodes in the tree
        - Packets processed
        - Average node accesses
        - Average packet processing time (nsecs)
        - Memory (Kbytes)
        - CPU Time (secs)
7. Medimos tiempo con clock_gettime(CLOCK_MONOTONIC_RAW, &finalTime)
8. Usamos la función printOutputLine(uint32_t IPAddress, int outInterface,
		     struct timespec *initialTime, struct timespec *finalTime,
		     double *searchingTime, int numberOfAccesses) o lo que ha hecho álvaro para escribir en el archivo de salida
9. Usamos la función printSummary(int NumberOfNodesInTrie, int processedPackets, double averageNodeAccesses, double averagePacketProcessingTime) para escribir la primera parte del report final y la función printMemoryTimeUsage() para imprimir memory and CPU time del report final
10. Fin del programa

Nota: Tenemos que crear vbles y calcular los argumentos que le vamos a pasar a la función printSummary (punto 8)
      También falta adaptar la función de lookup para hacer lookup de compress trie. Primero ver si funciona 
      normal(para ello tiene que funcionar todo lo demás xD) 

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
        /* En este caso pasar la info de uno a otro */
        root.next_hop = new.next_hop;
        return;
    }

    if (new.bit_that_matters) {
        /* Repetir el trabajo en la rama izquierda */
        if (!root.right) root.right = context_alloc(appropriate_node);
        insert_node(root.right, new);
    } else {
        /* Repetir el trabajo en la rama derecha */
        if (!root.left) root.left = context_alloc(appropriate_node);
        insert_node(root.left, new);
    }
}

create_trie(FILE *file_path) :: node *
{
    node_pool nodes = {0}; // La piscina de nodos tiene solo el scope de esta función.
    read_entire_file(file_path, &nodes);
    node *root = context_alloc(&(node) {0}); // root va a ser 0.0.0.0/0. Alojada en otro contexto.
    for (auto i = 1; i < nodes.size; ++i)
        insert_node(root, nodes[i]);
}
```

## Profundidad del árbol

```
count_trie(node *root, size_t total = 0) :: size_t      // Empezamos con 0 para sumar 1 en root
{
    total += 1;                                         // Contar nodo actual
    if (root.left) total += count_trie(root.left);      // Contar ramas a la izquierda
    if (root.right) total += count_trie(root.right);    // Contar ramas a la derecha
    return total;
}
```

## Comprimir el árbol

```
compress_trie(node *root) :: cnode *
{
    cnode *alt_root = root; // Hay que adaptar el nodo
    if (alt_root.left == NULL && alt_root.right == NULL) // Caso final
        return alt_root;
    if (only_left_branch(alt_root) && does_not_have_iface(alt_root)) { // Solo rama izquierda y se puede comprimir
        alt_root.next_hop = alt_root.left.next_hop;
        alt_root.compressed_levels += 1;
    } else if (only_right_branch(alt_root) && does_not_have_iface(alt_root) { //  Solo rama dcha y se puede comprimir
        alt_root.next_hop = alt_root.right.next_hop;
        alt_root.compressed_levels += 1;
    } else { // Seguir probando con las ramas de abajo
        compress_trie(alt_root.left);
        compress_trie(alt_root.right);
    }
}
```
