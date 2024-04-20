#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include "nand.h"

/**
 * These states of the enum represent the following situations:
 * CLEARED: the gate is ready to be evaluated,
 * VISITED: the gate is in the process of being evaluated,
 * CALCULATED: evaluation was successful; sig_out and path_length are set,
 * FAILED: evaluation failed; the gate cannot determine its output. 
*/
typedef enum {
    CLEARED,
    VISITED,
    CALCULATED,
    FAILED
} gate_state;

// Used to mark what's currently stored in the union declared lower
typedef enum {
    GATE,
    SIGNAL,
    NONE
} input_type;

/**  
 * This struct represents a bind between two gates; it points to some gate 
 * and tells at which index in its appropriate array we are; this helps with
 * connecting and disconnecting. 
*/
typedef struct {
    size_t array_idx;
    nand_t *gate;
} bind_t;

/**
 * Gate input might be one of the two: another gate or a bool signal, that's
 * why it's nice to wrap them into one struct and switch between them if
 * necessary.
*/
typedef struct {
    input_type type;
    union {
        bool const *signal;
        bind_t bind;
    };
} input_t;

/**  
 * The gate functions as follows:
 * -the inputs array is of fixed size input_num,
 * -the outputs array size changes and there might be empty spaces in the array,
 * -both sig_out storing the ouput and path_length storing the critical path
 * are valid ONLY WHEN state == CALCULATED.
*/
struct nand {
    unsigned input_num;
    size_t output_num;
    input_t *inputs;
    bind_t *outputs;

    bool sig_out;
    size_t path_length;
    gate_state state;
};

// Simple max only for ssize_t
static ssize_t max(ssize_t x, ssize_t y) {
    return x > y ? x : y;
}

// Disconnects the output of index k from the gate g
static void _nand_disconnect_output(nand_t *g, size_t k) { 
    /* We suppose that we get correct input, otherwise something went wrong
       elsewhere */
    assert(g != NULL);
    assert(k < g->output_num);
            
    // Gets the pointer to the bind we want to reset
    bind_t *output_bind = &(g->outputs[k]);
    // Gets the gate we are the input of
    nand_t *target_gate = output_bind->gate;
    // It might be an empty output block, if that's the case we can return
    if (target_gate == NULL) {
        return;
    }
    // This is the index we are at in the inputs array of the target gate
    size_t our_idx = output_bind->array_idx;
    // Resets the input that corresponds to us in the target gate
    target_gate->inputs[our_idx].type = NONE;
    // Resets our bind
    output_bind->gate = NULL;
}

// Wraps _nand_disconnect_output to work both ways
static void _nand_disconnect_input(input_t input) {
    if (input.type == GATE) {
        _nand_disconnect_output(input.bind.gate, input.bind.array_idx);
    }
}

/** 
 * Inner recursive function that evaluates the output and the path for just
 * one gate. It returns the length of the critical path, or -1 if it couldn't
 * be evaluated. Whatever the result, it leaves the gates in some state that
 * cannot be left after the outer nand_evaluate is finished, because it might
 * no longer be true, so _evaluate_cleanup must be called.
 * */ 
static ssize_t _nand_evaluate(nand_t *g) {
    assert(g != NULL); // Should never happen

    if (g->state == CALCULATED) {
        return g->path_length;
    }
    if (g->state == FAILED) {
        return -1;
    }
    // We try evaluating on a gate that is being evaluated - a cycle detected
    if (g->state == VISITED) { 
        g->state = FAILED; 
        return -1;
    }
    // Special case for a gate with no inputs
    if (g->input_num == 0) {
        g->state = CALCULATED;
        g->sig_out = false;
        return (g->path_length = 0);
    }

    // this gate is CLEARED so now we set it to VISITED and check all the inputs
    g->state = VISITED;
    unsigned true_counter = 0;
    g->path_length = 0;

    for (unsigned i = 0; i < g->input_num; i++) {
        input_t input = g->inputs[i];
        if (input.type == NONE) { // All inputs must be set
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
                true_counter++;
            }
        } 
        // The last option is a signal
        else if (*(input.signal) == true) {
            true_counter++;
        } 
    }
    // If all the inputs are true then the output is false
    g->sig_out = true;
    if (true_counter == g->input_num) {
        g->sig_out = false; 
    }
    g->state = CALCULATED;

    return ++(g->path_length);
}

// This function sets all gates to CLEARED going upstream
static void _evaluate_cleanup(nand_t *g) {
    assert(g != NULL); // Should never happen

    // Nothing to do here
    if (g->state == CLEARED) {
        return;
    }
    // We first set our state to CLEARED so we are never stuck in a cycle
    g->state = CLEARED;
    for (unsigned i = 0; i < g->input_num; i++) {
        // Only gates need to be cleaned up
        if (g->inputs[i].type == GATE) {
            _evaluate_cleanup(g->inputs[i].bind.gate);
        }
    }
}

// Creates a new gate with n inputs
nand_t *nand_new(unsigned n) {
    nand_t *new_gate = (nand_t *)malloc(sizeof(nand_t));
    if (new_gate == NULL) {
        errno = ENOMEM;
        return NULL;
    } 
    
    new_gate->inputs = (input_t *)malloc(n * sizeof(input_t));
    if (new_gate->inputs == NULL) {
        free(new_gate);
        errno = ENOMEM;
        return NULL;
    }

    // 1 output for now
    new_gate->outputs = (bind_t *)malloc(sizeof(bind_t));
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
    
    new_gate->outputs[0].gate = NULL;

    // The state should always be CLEARED for every gate before evaluating
    new_gate->state = CLEARED;
    new_gate->input_num = n;
    new_gate->output_num = 1;
    // sig_out and path_length need not be initialized
    return new_gate;
}

// Deletes the gate g
void nand_delete(nand_t *g) {
    // Nothing to do
    if (g == NULL) {
        return;
    }
    
    // We disconnect ourselves from other gates so no gate has invalid pointers
    for (unsigned i = 0; i < g->input_num; i++) {
        _nand_disconnect_input(g->inputs[i]);
    }
    for (size_t i = 0; i < g->output_num; i++) {
        _nand_disconnect_output(g, i);
    }
    
    free(g->inputs);
    free(g->outputs);
    free(g);
}

// Connects the g_out as the input of index k for the g_in gate
int nand_connect_nand(nand_t *g_out, nand_t *g_in, unsigned k) {
    if (g_out == NULL || g_in == NULL || k >= g_in->input_num) {
        errno = EINVAL;
        return -1;
    }

    // If there is a free place for a connection, find it
    ssize_t free_place = -1;
    for (size_t i = 0; i < g_out->output_num; i++) {
        if (g_out->outputs[i].gate == NULL) {
            free_place = i;
            break;
        }
    }

    // We haven't found a free place for a connection
    if (free_place == -1) {
        size_t old_size = g_out->output_num;
        // We try doubling the amount of outputs
        bind_t *new_outputs = realloc(g_out->outputs,\
                                      2 * old_size * sizeof(bind_t));
        if (new_outputs == NULL) {
            errno = ENOMEM;
            return -1;
        }
        g_out->outputs = new_outputs;
        g_out->output_num *= 2;

        // Set all new outputs to default but not the first new
        for (size_t i = old_size + 1; i < old_size * 2; i++) {
            g_out->outputs[i].gate = NULL;
        }
        // This is the first free place we have
        free_place = old_size;
    }

    // Set up our bind
    g_out->outputs[free_place].array_idx = k;
    g_out->outputs[free_place].gate = g_in;

    // Set up g_in's bind
    _nand_disconnect_input(g_in->inputs[k]);
    g_in->inputs[k].type = GATE;
    g_in->inputs[k].bind.array_idx = free_place;
    g_in->inputs[k].bind.gate = g_out;

    return 0;
}

// Connects a bool signal s as the input of index k for the gate g
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

// Counts all the valid outputs for the gate g
ssize_t nand_fan_out(nand_t const *g) {
    if (g == NULL) {
        errno = EINVAL;
        return -1;
    }

    // Count all the real gates and not the holes
    size_t count = 0;
    for (size_t i = 0; i < g->output_num; i++) {
        if (g->outputs[i].gate != NULL) {
            count++;
        }
    }

    return count;
}

// Gets the input of index k for the gate g
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
        // We explicitly cast to a pointer to a non const bool
        return (bool *)g->inputs[k].signal;
    }
    // The only left option is a gate
    return g->inputs[k].bind.gate;
}

// We get the output of index k for the gate g
nand_t* nand_output(nand_t const *g, ssize_t k) {
    // We first need to check if k < 0 before safely casting to size_t
    // Why is k signed here?
    if (g == NULL || k < 0 || (size_t)k >= g->output_num) {
        return NULL;
    }

    // We count the index of the output excluding holes
    ssize_t counter = 0;

    for (size_t i = 0; i < g->output_num; i++) {
        if (g->outputs[i].gate != NULL) {
            if (counter == k) {
                return g->outputs[i].gate;
            }
            counter++;
        }
    }
    // There weren't enough outputs
    return NULL;
}

/**
 * Evaluate outputs for the array of gates g which is of size m, then put these
 * results into the array s of the same size and return the critical path length
*/
ssize_t nand_evaluate(nand_t **g, bool *s, size_t m) {
    if (m == 0 || g == NULL || s == NULL) {
        errno = EINVAL;
        return -1;
    }
    // We also need to check whether any of the gates are NULL
    for (size_t i = 0; i < m; i++) {
        if (g[i] == NULL) {
            errno = EINVAL;
            return -1;
        }
    }

    ssize_t path_length = 0;
    // This flag tells us whether the evaluation was successful or not
    bool failed = false;

    for (size_t i = 0; i < m; i++) {
        ssize_t new_length = _nand_evaluate(g[i]);
        // Return value of -1 means the evaluation failed
        if (new_length == -1) {
            failed = true;
            break;
        }
        // Evaluation was successful for this gate so we can set the output
        s[i] = g[i]->sig_out;
        path_length = max(path_length, new_length);
    }
    // Clean up the gates to their default CLEARED state regardless
    for (size_t i = 0; i < m; i++) {
        _evaluate_cleanup(g[i]);
    }

    if (failed) {
        errno = ECANCELED;
        return -1;
    }

    return path_length;
}