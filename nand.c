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
    ssize_t array_idx;
    nand_t *gate;
} nand_bind_ptr;

typedef struct {
    input_type type;
    union {
        bool *signal;
        nand_bind_ptr bind;
    };
} input_t;

struct nand {
    unsigned input_num;
    unsigned output_num;
    bool state;
    input_t *inputs;
    nand_bind_ptr *outputs;
};


nand_t *nand_new(unsigned n) { // OK!
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
    // 1 output for now
    new_gate->outputs = (nand_bind_ptr *)malloc(sizeof(nand_bind_ptr));
    if (new_gate->outputs == NULL) {
        free(new_gate);
        free(new_gate->inputs);
        errno = ENOMEM;
        return NULL;
    }

    // Initialize to NONEs for now
    for (unsigned i = 0; i < n; i++) {
        new_gate->inputs[i].type = NONE;
    }
    
    new_gate->outputs[0].array_idx = -1;
    new_gate->outputs[0].gate = NULL;

    new_gate->input_num = n;
    new_gate->output_num = 0;
    return new_gate;
}

// Disconnects gate g output, where k is the output number
void _nand_disconnect_output(nand_t *g, unsigned k) { // OK!
    if (g == NULL) {
        return;
    }
    assert(k < g->output_num); // should never happen

    nand_bind_ptr output_bind = g->outputs[k];
    if (output_bind.gate == NULL) {
        return;
    }
    //TODO: size of the array of pointers output doesnt change but its only
    // partially used; when connecting new output first check for NULL entries.
    g->outputs[k].array_idx = -1;
    g->outputs[k].gate = NULL;
    ssize_t our_idx = output_bind.array_idx;
    output_bind.gate->inputs[our_idx].type = NONE;
}

void _nand_disconnect_input(input_t input) { // OK!
    if (input.type == GATE) {
        _nand_disconnect_output(input.bind.gate, input.bind.array_idx);
    }
}

void nand_delete(nand_t *g) { //OK!
    if (g == NULL) {
        return;
    }
    
    for (int i = 0; i < g->output_num; i++) {
        _nand_disconnect_output(g, i);
    }
    for (int i = 0; i < g->input_num; i++) {
        _nand_disconnect_input(g->inputs[i]);
    }
    free(g->inputs);
    free(g->outputs);
    free(g);
}

int nand_connect_nand(nand_t *g_out, nand_t *g_in, unsigned k) { //OK!
    if (g_out == NULL || g_in == NULL || k >= g_in->input_num) {
        errno = EINVAL;
        return -1;
    }

    // If there is a free place for a connection, find it
    unsigned free_place = -1;
    for (int i = 0; i < g_out->output_num; i++) {
        if (g_out->outputs[i].array_idx == -1) {
            free_place = i;
            break;
        }
    }
    // We haven't found a free connection
    if (free_place == -1) {
        unsigned old_size = g_out->output_num;
        nand_bind_ptr *new_outputs = realloc(g_out->outputs, old_size * 2);
        if (new_outputs == NULL) {
            errno = ENOMEM;
            return -1;
        }
        g_out->outputs = new_outputs;
        g_out->output_num *= 2;
        // Set all new outputs to default but not the first new
        for (int i = old_size + 1; i < old_size * 2; i++) {
            g_out->outputs[i].array_idx = -1;
            g_out->outputs[i].gate = NULL;
        }
        free_place = old_size;
    }

    g_out->outputs[free_place].array_idx = k;
    g_out->outputs[free_place].gate = g_in;

    _nand_disconnect_input(g_in->inputs[k]);
    g_in->inputs[k].type = GATE;
    g_in->inputs[k].bind.array_idx = free_place;
    g_in->inputs[k].bind.gate = g_out;
    return 0;
}

int nand_connect_signal(bool const *s, nand_t *g, unsigned k) {
    if (g == NULL || s == NULL || k >= g->input_num) {
        errno = EINVAL;
        return -1;
    }
    _nand_disconnect_input(g->inputs[k]);
    g->inputs[k].type = SIGNAL;
    g->inputs[k].signal = s;
    return 1;
}

ssize_t nand_fan_out(nand_t const *g) {
    if (g == NULL) {
        errno = EINVAL;
        return -1;
    }
    ssize_t free_count = 0;
    for (int i = 0; i < g->output_num; i++) {
        if (g->outputs[i].array_idx == -1) {
            free_count++;
        }
    }
    return g->output_num - free_count;
}

void* nand_input(nand_t const *g, unsigned k) {
    if (g == NULL || k >= g->input_num) {
        errno = EINVAL;
        return NULL;
    }
    if (g->inputs[k].type == NONE) {
        errno = 0;
        return NULL;
    }
    if (g->inputs[k].type == SIGNAL) {
        return g->inputs[k].signal;
    }
    return g->inputs[k].bind.gate;
}

nand_t* nand_output(nand_t const *g, ssize_t k) {
    if (g == NULL || k >= g->output_num) {
        return NULL;
    }
    ssize_t counter = 0;
    for (int i = 0; i < g->output_num; i++) {
        if (g->outputs[i].array_idx != -1) {
            if (++counter == k) {
                return g->outputs[i].gate;
            }
        }
    }
    return NULL;
}