#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include "nand.h"

typedef enum {
    GATE,
    SIGNAL,
    NONE
} input_type;

typedef struct {
    input_type type;
    union {
        bool *signal;
        nand_t *gate;
    };
} input_t;

struct nand {
    unsigned input_num;
    unsigned output_num;
    bool state;
    input_t *inputs;
    nand_t **outputs;
};


nand_t *nand_new(unsigned n) {
    // Try allocating memory for the gate
    nand_t *new_gate = (nand_t *)malloc(sizeof(nand_t));
    if (new_gate == NULL) {
        errno = ENOMEM;
        return NULL;
    } 
    // Try allocating memory for the inputs array
    new_gate->inputs = (input_t *)malloc(n * sizeof(input_t));
    if (new_gate->inputs == NULL) {
        free(new_gate);
        errno = ENOMEM;
        return NULL;
    }
    // Initialize to NONEs for now
    for (unsigned i = 0; i < n; i++) {
        new_gate->inputs[i].type = NONE;
    }
    new_gate->outputs = NULL;
    new_gate->input_num = n;
    new_gate->output_num = 0;
    return new_gate;
}

// Disconnects gate g_out output, where k is the output number
void _nand_disconnect_nand(nand_t *g_out, unsigned k) {
    nand_t *output_gate = g_out->outputs[k];
    if (output_gate == NULL) {
        return;
    }
    //TODO: size of the array of pointers output doesnt change but its only
    // partially used; when connecting new output first check for NULL entries.
    g_out->outputs[k] = NULL;
    for (int i = 0; i < output_gate->input_num; i++) {
        input_t *current_input = &(output_gate->inputs[i]);
        if (current_input->type == GATE && current_input->gate == g_out) {
            current_input->type = NONE;
            // garbage stays at the location
            return;
        }
    }
    assert(false);
}

void nand_delete(nand_t *g) {
    if (g == NULL) {
        return;
    }
    
    for (int i = 0; i < g->output_num; i++) {
        _nand_disconnect_nand(g, i);
    }
    free(g->inputs);
    free(g->outputs);
    free(g);
}

int nand_connect_nand(nand_t *g_out, nand_t *g_in, unsigned k) {
    
}