#include "nand.h"
#include <assert.h>

int main() {
  // Create 3 nands that are connected to pairs of bool signals
  nand_t* nand_arr[3];
  bool signal[6] = {0, 0, 0, 0, 0, 0};

  for (int i = 0; i < 3; i++) {
    nand_arr[i] = nand_new(2);
    nand_connect_signal(&signal[2 * i], nand_arr[i], 0);
    nand_connect_signal(&signal[2 * i + 1], nand_arr[i], 1);
  }

  // All previous gates are connected to this one
  nand_t* master_nand = nand_new(3);
  for (int i = 0; i < 3; i++) {
    nand_connect_nand(nand_arr[i], master_nand, i);
  }

  for (int i = 0; i < 3; i++) {
    // Each gate has 1 valid output
    assert(nand_fan_out(nand_arr[i]) == 1);
  }

  bool outputs[3];

  // For each of the three gates the critical path is 1
  assert(nand_evaluate(nand_arr, outputs, 3) == 1);

  for (int i = 0; i < 3; i++) {
    // Should all be true because all the gate inputs are false
    assert(outputs[i] == true);
  }

  bool master_output;

  // Critical path from master is 2
  assert(nand_evaluate(&master_nand, &master_output, 1) == 2);

  // All gates connected to master give out true so this should be false
  assert(master_output == false);

  for (int i = 0; i < 3; i++) {
    nand_delete(nand_arr[i]);
  }
  nand_delete(master_nand);

  return 0;
}