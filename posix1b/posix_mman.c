/*-< POSIX_MMAN.C >-------------------------------------------------*--------*/
/* POSIX.1b                   Version 1.0        (c) 1998  GARRET   *     ?  */
/* (POSIX.1b implementation for Linux)                              *   /\|  */
/*                                                                  *  /  \  */
/*                          Created:     25-Aug-98    K.A. Knizhnik * / [] \ */
/*                          Last update: 27-Aug-98    K.A. Knizhnik * GARRET */
/*------------------------------------------------------------------*--------*/
/* Stubs for shared object functions                                *        */
/*------------------------------------------------------------------*--------*/

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include "posix_mman.h"

int shm_open(const char *name, int oflag, mode_t mode)
{
    return open(name, oflag, mode);
}

int shm_unlink(const char *name)
{
    return unlink(name);
}

