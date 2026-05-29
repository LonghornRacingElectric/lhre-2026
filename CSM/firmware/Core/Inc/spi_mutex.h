#ifndef SPI_MUTEX_H
#define SPI_MUTEX_H
#include "cmsis_os.h"

extern osMutexId_t spi1_mutex;
void spi_mutex_init(void);
#endif