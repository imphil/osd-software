from cutil cimport va_list
from libc.stdint cimport uint16_t

cdef extern from "osd/osd.h":
    ctypedef int osd_result

    struct osd_log_ctx:
        pass

    ctypedef void (*osd_log_fn)(osd_log_ctx *ctx,
                           int priority, const char *file,
                           int line, const char *fn,
                           const char *format, va_list args);

    osd_result osd_log_new(osd_log_ctx **ctx,
                       int log_priority,
                       osd_log_fn log_fn)

    void osd_log_free(osd_log_ctx **ctx)

    void osd_log_set_fn(osd_log_ctx *ctx, osd_log_fn log_fn)

    void osd_log_set_caller_ctx(osd_log_ctx *ctx, void *caller_ctx)

    void* osd_log_get_caller_ctx(osd_log_ctx *ctx)



cdef extern from "osd/packet.h":
    struct osd_packet:
        pass

    cdef enum osd_packet_type:
        pass

    osd_result osd_packet_new(osd_packet **packet, size_t size_data_words)

    void osd_packet_free(osd_packet **packet)

    unsigned int osd_packet_get_dest(const osd_packet *packet)

    unsigned int osd_packet_get_src(const osd_packet *packet)

    unsigned int osd_packet_get_type(const osd_packet*packet)

    unsigned int osd_packet_get_type_sub(const osd_packet*packet)

    osd_result osd_packet_set_header(osd_packet* packet,
                                            const unsigned int dest,
                                            const unsigned int src,
                                            const osd_packet_type type,
                                            const unsigned int type_sub)

    void osd_packet_to_string(const osd_packet *packet, char** str)

cdef extern from "osd/hostmod.h":
    struct osd_hostmod_ctx:
        pass

    osd_result osd_hostmod_new(osd_hostmod_ctx **ctx, osd_log_ctx *log_ctx)

    void osd_hostmod_free(osd_hostmod_ctx **ctx)

    osd_result osd_hostmod_connect(osd_hostmod_ctx *ctx,
                                   const char* host_controller_address)

    osd_result osd_hostmod_disconnect(osd_hostmod_ctx *ctx)

    osd_result osd_hostmod_reg_read(osd_hostmod_ctx *ctx,
                                    void *result,
                                    uint16_t diaddr,
                                    uint16_t reg_addr,
                                    int reg_size_bit,
                                    int flags)

    osd_result osd_hostmod_reg_write(osd_hostmod_ctx *ctx,
                                     const void *data,
                                     uint16_t diaddr, uint16_t reg_addr,
                                     int reg_size_bit, int flags)

    uint16_t osd_hostmod_get_diaddr(osd_hostmod_ctx *ctx)
