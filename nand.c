#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include "nand.h"

typedef enum {
    CLEARED,
    VISITED,
    CALCULATED,
    FAILED
} gate_state;

typedef enum {
    GATE,
    SIGNAL,
    NONE
} input_type;

typedef struct {
    ssize_t array_idx;
    nand_t *gate;
} nand_bind;

typedef struct {
    input_type type;
    union {
        bool const *signal;
        nand_bind bind;
    };
} input_t;

struct nand {
    unsigned input_num;
    unsigned output_num;
    bool sig_out;
    ssize_t path_length;
    gate_state state;
    input_t *inputs;
    nand_bind *outputs;
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
    new_gate->outputs = (nand_bind *)malloc(sizeof(nand_bind));
    if (new_gate->outputs == NULL) {
        free(new_gate->inputs);
        free(new_gate);
        errno = ENOMEM;
        return NULL;
    }

    // Initialize to NONEs for now
    for (unsigned i = 0; i < n; i++) {
        new_gate->inputs[i].type = NONE;
    }
    
    new_gate->outputs[0].array_idx = -1;
    new_gate->outputs[0].gate = NULL;

    // The state should always be CLEARED for every gate before evaluating
    new_gate->state = CLEARED;
    new_gate->path_length = -1;
    new_gate->input_num = n;
    new_gate->output_num = 0;
    return new_gate;
}

// Disconnects gate g output, where k is the output number
static void _nand_disconnect_output(nand_t *g, unsigned k) { // OK!
    /* We suppose that we get correct input, otherwise something went wrong
       elsewhere */
    assert(g != NULL);
    assert(k < g->output_num);
            
    // Gets the pointer to the bind we want to reset
    nand_bind *output_bind = &(g->outputs[k]);
    // Gets the gate we are the input of
    nand_t *target_gate = output_bind->gate;
    // It might be an empty output block, if that's the case we can return
    if (target_gate == NULL) {
        return;
    }
    // This is the index we are at in the inputs array of the target gate
    ssize_t our_idx = output_bind->array_idx;
    // Resets the input that corresponds to us in the target gate
    target_gate->inputs[our_idx].type = NONE;
    // Resets our bind
    output_bind->array_idx = -1;
    output_bind->gate = NULL;
}

static void _nand_disconnect_input(input_t input) { // OK!
    if (input.type == GATE) {
        _nand_disconnect_output(input.bind.gate, input.bind.array_idx);
    }
}

void nand_delete(nand_t *g) { //OK!
    if (g == NULL) {
        return;
    }
    
    for (unsigned i = 0; i < g->output_num; i++) {
        _nand_disconnect_output(g, i);
    }
    for (unsigned i = 0; i < g->input_num; i++) {
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
    ssize_t free_place = -1;
    for (unsigned i = 0; i < g_out->output_num; i++) {
        if (g_out->outputs[i].gate = NULL) {
            free_place = i;
            break;
        }
    }
    // We haven't found a free connection
    if (free_place == -1) {
        unsigned old_size = g_out->output_num;
        nand_bind *new_outputs = realloc(g_out->outputs, old_size * 2);
        if (new_outputs == NULL) {
            errno = ENOMEM;
            return -1;
        }
        g_out->outputs = new_outputs;
        g_out->output_num *= 2;
        // Set all new outputs to default but not the first new
        for (unsigned i = old_size + 1; i < old_size * 2; i++) {
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
    return 0;
}

ssize_t nand_fan_out(nand_t const *g) {
    if (g == NULL) {
        errno = EINVAL;
        return -1;
    }
    unsigned free_count = 0;
    for (unsigned i = 0; i < g->output_num; i++) {
        if (g->outputs[i].gate == NULL) {
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
    for (unsigned i = 0; i < g->output_num; i++) {
        if (g->outputs[i].gate != NULL) {
            if (counter == k) {
                return g->outputs[i].gate;
            }
            counter++;
        }
    }
    return NULL;
}

static ssize_t _nand_evaluate(nand_t *g) {
    assert(g != NULL); // Something went terribly wrong
    if (g->state == CALCULATED) {
        return g->path_length;
    }
    if (g->state == FAILED) {
        return -1;
    }
    if (g->state == VISITED) { // Just for consistency
        g->state = FAILED;
        return -1;
    }
    if (g->input_num == 0) {
        g->state = CALCULATED;
        g->sig_out = false;
        return (g->path_length = 0);
    }
    // this gate is CLEARED so now we set it to VISITED and check all inputs
    g->state = VISITED;
    g->sig_out = true; // if any of the inputs is true then it will be false
    g->path_length = 0;
    for (int i = 0; i < g->input_num; i++) {
        input_t input = g->inputs[i];
        if (input.type == NONE) {
            g->state = FAILED;
            return -1;
        }
        if (input.type == GATE) {
            nand_t *gate = input.bind.gate;
            ssize_t outcome = _nand_evaluate(gate);
            if (outcome == -1) {  // Evaluate on the input failed
                g->state = FAILED;
                return -1;
            }
            g->path_length = max(g->path_length, outcome);
            if (gate->sig_out == true) {
                g->sig_out = false;
            }
        } else { // We have a signal
            if (*(input.signal) == true) {
                g->sig_out = false;
            }
        } 
    }
    g->state = CALCULATED;
    return ++(g->path_length);
}

static void _evaluate_cleanup(nand_t *g) {
    assert(g != NULL);
    if (g->state == CLEARED) {
        return;
    }
    g->state = CLEARED;
    for (int i = 0; i < g->input_num; i++) {
        if (g->inputs[i].type == GATE) {
            _evaluate_cleanup(g->inputs[i].bind.gate);
        }
    }
    return;
}

ssize_t nand_evaluate(nand_t **g, bool *s, size_t m) {
    if (m == 0) {
        errno = EINVAL;
        return -1;
    }
    for (int i = 0; i < m; i++) {
        if (g[i] == NULL) {
            errno = EINVAL;
            return -1;
        }
    }

    ssize_t path_length = 0;
    bool failed = false;
    for (int i = 0; i < m; i++) {
        ssize_t new_length = _nand_evaluate(g[i]);
        if (new_length == -1) {
            failed = true;
            break;
        }
        s[i] = g[i]->sig_out;
        path_length = max(path_length, new_length);
    }
    for (int i = 0; i < m; i++) {
        _evaluate_cleanup(g[i]);
    }
    if (failed) {
        errno = ECANCELED;
        return -1;
    }
    return path_length;
}
// Also, types are a little fucked up (ssize_t vs unsigned)
// We cast from void* to const bool* is that ok? (we have to cast at least once)