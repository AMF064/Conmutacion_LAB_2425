#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <assert.h>
#include <stdint.h>
#include "io.h"
#include "node.h"

//En mi caso es necesario que lo primero sea definir _POSIX_C_SOURCE 200809L para usar funciones POSIX de nivel 2008 o superior.
//asi el compilador no da error en la función gettime() ni en la macro CLOCK_MONOTONIC_RAW

extern int node_count;

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
    char *routing_file_path = args.fib_file;
    char *input_file = args.input_packet_file;
    int result = initializeIO(routing_file_path, input_file);
    if (result < 0) {
        printIOExplanationError(result);
        return 1;
    }
    node_t root = create_trie();
    if (!root) {
        free_nodes();
        freeIO();
        return 1;
    }

#ifdef DEBUG
    if (output_graphviz("out_uncompressed.gv", root) < 0) {
        free_nodes();
        freeIO();
        return 1;
    }
#endif

    root = compress_trie(root);
    uint32_t ip;
    int iface, accesses;
    int processed_packets = 0;
    double total_accesses = 0;
    double total_time = 0;
    double searching_time = 0.0;
    struct timespec start, end;


    while (readInputPacketFileLine(&ip) == OK) {
        accesses = 0;

        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        iface = lookup(root, ip, &accesses);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);

        printOutputLine(ip, iface, &start, &end, &searching_time, accesses);

        total_accesses += accesses;
        total_time += searching_time;
        processed_packets++;
    }

    //Estadísticas finales
    double average_accesses = 0;
    double average_time = 0;

    if (processed_packets > 0) {
        average_accesses = total_accesses / processed_packets;
        average_time = total_time / processed_packets;
    }
    printSummary(node_count, processed_packets, average_accesses, average_time);


    int return_value = 0;
#ifdef DEBUG
    if (output_graphviz("out_compressed.gv", root) < 0)
        return_value = 1;
#endif


    free_nodes();
    freeIO();
    return return_value;
}
