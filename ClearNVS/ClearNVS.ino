/*
 * Upload this sketch to clear any saved data from non-volatile storage (nvs).
 *
 * UPLOAD A NEW SKETCH AFTER LOADING THIS ONE OR THIS WILL BE RAN ON EVERY BOARD START UP
 */

#include <nvs_flash.h>

void setup() {
  nvs_flash_erase(); // erase the NVS partition and...
  nvs_flash_init(); // initialize the NVS partition.
  while(true);
}

void loop() {}