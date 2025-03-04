/*-< POSIX_MMAN.H >-------------------------------------------------*--------*/
/* POSIX.1b                   Version 1.0        (c) 1998  GARRET   *     ?  */
/* (POSIX.1b implementation for Linux)                              *   /\|  */
/*                                                                  *  /  \  */
/*                          Created:     25-Aug-98    K.A. Knizhnik * / [] \ */
/*                          Last update: 27-Aug-98    K.A. Knizhnik * GARRET */
/*------------------------------------------------------------------*--------*/
/* Interface of shared object functions                             *        */
/*------------------------------------------------------------------*--------*/

#ifdef __cplusplus
extern "C" { 
#endif

#include <sys/types.h>

int shm_open(const char *name, int oflag, mode_t mode);

int shm_unlink(const char *name);

#ifdef __cplusplus
} 
#endif

