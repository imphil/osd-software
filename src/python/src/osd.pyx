cimport cosd
from cutil cimport va_list, vasprintf
from libc.stdlib cimport free
from libc.stdint cimport uint16_t
import logging

cdef void log_cb(cosd.osd_log_ctx *ctx, int priority, const char *file,
                 int line, const char *fn, const char *format,
                 va_list args):

    # Get log message as unicode string
    cdef char* c_str = NULL
    len = vasprintf(&c_str, format, args)
    if len == -1:
        raise MemoryError()
    try:
        u_msg = c_str[:len].decode('UTF-8')
    finally:
        free(c_str)

    u_file = file.decode('UTF-8')
    u_fn = fn.decode('UTF-8')

    # create log record and pass it to the Python logger
    logger = logging.getLogger(__name__)
    record = logging.LogRecord(name = __name__,
                               level = loglevel_syslog2py(priority),
                               pathname = u_file,
                               lineno = line,
                               func = u_fn,
                               msg = u_msg,
                               args = '',
                               exc_info = None)

    logger.handle(record)
    logger.warning("something else")

cdef loglevel_py2syslog(py_level):
    """
    Convert Python logging severity levels to syslog levels as defined in
    syslog.h
    """
    if py_level == logging.CRITICAL:
        return 2 # LOG_CRIT
    elif py_level == logging.ERROR:
        return 3 # LOG_ERR
    elif py_level == logging.WARNING:
        return 4 # LOG_WARNING
    elif py_level == logging.INFO:
        return 6 # LOG_INFO
    elif py_level == logging.DEBUG:
        return 7 # LOG_DEBUG
    elif py_level == logging.NOTSET:
        return 0 # LOG_EMERG
    else:
        raise Exception("Unknown loglevel " + py_level)


cdef loglevel_syslog2py(syslog_level):
    """
    Convert syslog log severity levels, as defined in syslog.h, to Python ones
    """
    if syslog_level <= 2: # LOG_EMERG, LOG_ALERT, LOG_CRIT
        return logging.CRITICAL
    elif syslog_level == 3: # LOG_ERR
        return logging.ERROR
    elif syslog_level == 4: # LOG_WARNING
        return logging.WARNING
    elif syslog_level <= 6: # LOG_NOTICE, LOG_INFO
        return logging.INFO
    elif syslog_level == 6: # LOG_DEBUG
        return logging.DEBUG
    else:
        raise Exception("Unknown loglevel " + syslog_level)

cdef class Log:
    cdef cosd.osd_log_ctx* _cself

    def __cinit__(self):
        logger = logging.getLogger(__name__)
        py_loglevel = logger.getEffectiveLevel()
        syslog_loglevel = loglevel_py2syslog(py_loglevel)

        cosd.osd_log_new(&self._cself, syslog_loglevel, log_cb)
        if self._cself is NULL:
            raise MemoryError()

    def __dealloc(self):
        if self._cself is not NULL:
            cosd.osd_log_free(&self._cself)


cdef class Packet:
    cdef cosd.osd_packet* _cself

    def __cinit__(self):
        # XXX: don't make the length fixed! Might require API changes on C side
        cosd.osd_packet_new(&self._cself, 10)
        if self._cself is NULL:
            raise MemoryError()

    def __dealloc__(self):
        if self._cself is not NULL:
            cosd.osd_packet_free(&self._cself)

    @property
    def src(self):
        return cosd.osd_packet_get_src(self._cself)

    @property
    def dest(self):
        return cosd.osd_packet_get_dest(self._cself)

    @property
    def type(self):
        return cosd.osd_packet_get_type(self._cself)

    @property
    def type_sub(self):
        return cosd.osd_packet_get_type_sub(self._cself)

    def set_header(self, dest, src, type, type_sub):
        cosd.osd_packet_set_header(self._cself, dest, src, type, type_sub)

    def __str__(self):
        cdef char* c_str = NULL
        cosd.osd_packet_to_string(self._cself, &c_str)

        try:
            py_u_str = c_str.decode('UTF-8')
        finally:
            free(c_str)

        return py_u_str

cdef class Hostmod:
    cdef cosd.osd_hostmod_ctx* _cself

    def __cinit__(self, Log log):
        cosd.osd_hostmod_new(&self._cself, log._cself)
        if self._cself is NULL:
            raise MemoryError()

    def __dealloc__(self):
        if self._cself is not NULL:
            cosd.osd_hostmod_free(&self._cself)

    def connect(self, host_controller_address):
        py_byte_string = host_controller_address.encode('UTF-8')
        cdef char* c_host_controller_address = py_byte_string
        cosd.osd_hostmod_connect(self._cself, c_host_controller_address)

    def disconnect(self):
        cosd.osd_hostmod_disconnect(self._cself)

    def reg_read(self, diaddr, reg_addr, reg_size_bit = 16, flags = 0):
        cdef uint16_t outvalue
        if reg_size_bit != 16:
            raise Exception("XXX: Extend to support other sizes than 16 bit registers")

        cosd.osd_hostmod_reg_read(self._cself, &outvalue, diaddr, reg_addr,
                                  reg_size_bit, flags)

        return outvalue

    def reg_write(self, data, diaddr, reg_addr, reg_size_bit = 16, flags = 0):
        cosd.osd_hostmod_reg_write(self._cself, data, diaddr, reg_addr,
                                   reg_size_bit, flags)
