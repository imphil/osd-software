# these defines are not part of libc.stdio as provided by cython today
cdef extern from "<stdio.h>":
    ctypedef void * va_list
    int vasprintf(char **strp, const char* fmt, va_list ap)
