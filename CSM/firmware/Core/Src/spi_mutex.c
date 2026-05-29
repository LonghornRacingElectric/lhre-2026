#include "spi_mutex.h"
osMutexId_t spi1_mutex;


void spi_mutex_init(void) {
    spi1_mutex = osMutexNew(NULL);
}