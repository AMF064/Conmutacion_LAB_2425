#include <assert.h>
#include <stdint.h>
#include "io.h"
#include "node.h"

#define INPUT_FILE_PATH "prueba1.txt"
#define ROUTING_FILE_PATH "routing_table.txt"
int main(void)
{
    int return_value = 0;
    char routing_file_path[] = ROUTING_FILE_PATH;
    char input_file[] = INPUT_FILE_PATH;
    int result = initializeIO(routing_file_path, input_file);
    if (result < 0) {
        printIOExplanationError(result);
        return 1;
    }
    Node *root = create_trie();
    if (!root) {
        return_value = 1;
        goto out;
    }

    print_trie(stdout, root, 0);

out:
    free_nodes(root);
    freeIO();
    return return_value;
}
