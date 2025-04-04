#include <assert.h>
#include <stdint.h>
#include "io.h"
#include "node.h"

typedef struct {
    char *fib_file;
    char *input_packet_file;
} Args;

void usage(char *cmd, char *errmsg)
{
    fprintf(stderr, "Usage: %s <FIB> <InputPacketFile>\n", cmd);
    fputs(errmsg, stderr);
}

char *shift(int *ac, char ***av)
{
    char *result = **av;
    *av += 1;
    *ac -= 1;
    return result;
}

int parse_cmdline_opts(int argc, char **argv, Args *args)
{
    char *command = shift(&argc, &argv);
    if (!argc) {
        usage(command, "ERROR: no files provided\n");
        return -1;
    }
    args->fib_file = shift(&argc, &argv);
    if (!argc) {
        usage(command, "ERROR: no input packet file provided\n");
        return -1;
    }
    args->input_packet_file = shift(&argc, &argv);
    return 0;
}

int main(int argc, char *argv[])
{
    Args args = {0};
    if (parse_cmdline_opts(argc, argv, &args) < 0)
        return 1;
    int return_value = 0;
    char *routing_file_path = args.fib_file;
    char *input_file = args.input_packet_file;
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

    if (output_graphviz("out.gv", root) < 0)
        perror("fopen");

out:
    free_nodes(root);
    freeIO();
    return return_value;
}
