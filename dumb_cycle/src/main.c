typedef short i16;
typedef unsigned short u16;
typedef int i32;
typedef unsigned int u32;
typedef long i64;
typedef unsigned long u64;

u64 _syscall(
    u64 scid,
    u64 arg0,
    u64 arg1,
    u64 arg2,
    u64 arg3,
    u64 arg4,
    u64 arg5
);

enum sys {
    SYS_WRITE = 1,
    SYS_OPEN = 2,
    SYS_MMAP = 9,
    SYS_IOCTL = 16,
    SYS_EXIT = 60,
    SYS_MOUNT = 165,
};

enum std {
    STD_IN = 0,
    STD_OUT = 1,
    STD_ERR = 2,
};

i64 write(int fd, const char *buf, i64 len) {
    return _syscall(SYS_WRITE, fd, (u64)buf, len, 0, 0, 0);
}

enum open_flags {
    O_RDONLY = 0,
    O_WRONLY = 1,
    O_RDWR = 2,
};

int open(const char *fname, int flags, int mode) {
    return _syscall(SYS_OPEN, (u64)fname, flags, mode, 0, 0, 0);
}

enum mmap_prot {
    PROT_READ = 1,
    PROT_WRITE = 2,
};

enum mmap_flags {
    MAP_SHARED = 0x01,
    MAP_ANONYMOUS = 0x20,
};

void *mmap(void *start, i64 len, int prot, int flags, int fd, i64 off) {
    u64 ret = _syscall(SYS_MMAP, (u64)start, len, prot, flags, fd, off);

    if (ret > -4096UL) {
        return 0;
    }

    return (void *)ret;
}

enum ioctl_dir {
    IOCTL_DIR_RDONLY = 1,
    IOCTL_DIR_WRONLY = 2,
    IOCTL_DIR_RDWR = 3,
};

enum ioctl_type {
    IOCTL_TYPE_DRM = 'd',
};

enum drm_ioctl {
    DRM_IOCTL_MODE_GETRESOURCES = 0xA0,
    DRM_IOCTL_MODE_GETCRTC = 0xA1,
    DRM_IOCTL_MODE_SETCRTC = 0xA2,
    DRM_IOCTL_MODE_GETENCODER = 0xA6,
    DRM_IOCTL_MODE_GETCONNECTOR = 0xA7,
    DRM_IOCTL_MODE_ADDFB = 0xAE,
    DRM_IOCTL_MODE_CREATE_DUMB = 0xB2,
    DRM_IOCTL_MODE_MAP_DUMB = 0xB3,
};

u32 ioctl_op(u32 dir, u32 size, u32 type, u32 num) {
    return (num & 0xff) | ((type & 0xff) << 8) | ((size & 0x3fff) << 16) | (
        (dir & 0x3) << 30
    );
}

int ioctl(int fd, u32 op, void *data) {
    return _syscall(SYS_IOCTL, fd, op, (u64)data, 0, 0, 0);
}

void exit(int error_code) {
    _syscall(SYS_EXIT, error_code, 0, 0, 0, 0, 0);
}

int mount(
    const char *dev_name,
    const char *dir_name,
    const char *type,
    int flags,
    void *data
) {
    return _syscall(
        SYS_MOUNT,
        (u64)dev_name,
        (u64)dir_name,
        (u64)type,
        flags,
        (u64)data,
        0
    );
}

i64 strlen(const char *str) {
    i64 len = 0;
    for (const char *c = str; *c != 0; c++) {
        len++;
    }
    return len;
}

void panic(const char *msg) {
    write(STD_ERR, msg, strlen(msg));
    for (;;) {}
}

struct arena {
    char *beg;
    char *end;
};

void *alloc(struct arena *a, i64 size, i64 count) {
    if (size == 0 || count == 0) {
        return 0;
    }

    i64 padding = -(u64)a->beg & (16 - 1);
    i64 available = a->end - a->beg - padding;
    if (available < 0 || count > available / size) {
        panic("out of memory");
    }
    char *p = a->beg + padding;
    a->beg += padding + count * size;

    for (i64 i = 0; i < count * size; i++) {
        p[i] = 0;
    }

    return p;
}

struct drm_mode_card_res {
    u32 *fb_id_ptr;
    u32 *crtc_id_ptr;
    u32 *connector_id_ptr;
    u32 *encoder_id_ptr;
    u32 count_fbs;
    u32 count_crtcs;
    u32 count_connectors;
    u32 count_encoders;
    u32 min_width;
    u32 max_width;
    u32 min_height;
    u32 max_height;
};

struct drm_mode_card_res *drm_mode_get_res(int card_fd, struct arena *arena) {
    struct drm_mode_card_res last_res;
    struct arena temp_arena;
    struct drm_mode_card_res *res;

    do {
        temp_arena = *arena;

        res = alloc(&temp_arena, sizeof(*res), 1);

        int error = ioctl(
            card_fd,
            ioctl_op(
                IOCTL_DIR_RDWR,
                sizeof(*res),
                IOCTL_TYPE_DRM,
                DRM_IOCTL_MODE_GETRESOURCES
            ),
            res
        );

        if (error) {
            return 0;
        }

        last_res = *res;

        res->fb_id_ptr = alloc(
            &temp_arena,
            sizeof(*res->fb_id_ptr),
            res->count_fbs
        );
        res->crtc_id_ptr = alloc(
            &temp_arena,
            sizeof(*res->crtc_id_ptr),
            res->count_crtcs
        );
        res->connector_id_ptr = alloc(
            &temp_arena,
            sizeof(*res->connector_id_ptr),
            res->count_connectors
        );
        res->encoder_id_ptr = alloc(
            &temp_arena,
            sizeof(*res->encoder_id_ptr),
            res->count_encoders
        );

        error = ioctl(
            card_fd,
            ioctl_op(
                IOCTL_DIR_RDWR,
                sizeof(*res),
                IOCTL_TYPE_DRM,
                DRM_IOCTL_MODE_GETRESOURCES
            ),
            res
        );

        if (error) {
            return 0;
        }
    } while (
        last_res.count_fbs < res->count_fbs ||
        last_res.count_crtcs < res->count_crtcs ||
        last_res.count_encoders < res->count_encoders ||
        last_res.count_connectors < res->count_connectors
    );

    *arena = temp_arena;

    return res;
}

struct drm_mode_modeinfo {
    u32 clock;
    u16 hdisplay;
    u16 hsync_start;
    u16 hsync_end;
    u16 htotal;
    u16 hskew;
    u16 vdisplay;
    u16 vsync_start;
    u16 vsync_end;
    u16 vtotal;
    u16 vscan;

    u32 vrefresh;

    u32 flags;
    u32 type;
    char name[32];
};

struct drm_mode_get_connector {
    /** @encoders_ptr: Pointer to ``u32`` array of object IDs. */
    u32 *encoders_ptr;
    /** @modes_ptr: Pointer to struct drm_mode_modeinfo array. */
    struct drm_mode_modeinfo *modes_ptr;
    /** @props_ptr: Pointer to ``u32`` array of property IDs. */
    u32 *props_ptr;
    /** @prop_values_ptr: Pointer to ``u64`` array of property values. */
    u64 *prop_values_ptr;

    /** @count_modes: Number of modes. */
    u32 count_modes;
    /** @count_props: Number of properties. */
    u32 count_props;
    /** @count_encoders: Number of encoders. */
    u32 count_encoders;

    /** @encoder_id: Object ID of the current encoder. */
    u32 encoder_id;
    /** @connector_id: Object ID of the connector. */
    u32 connector_id;
    /**
    * @connector_type: Type of the connector.
    *
    * See DRM_MODE_CONNECTOR_* defines.
    */
    u32 connector_type;
    /**
    * @connector_type_id: Type-specific connector number.
    *
    * This is not an object ID. This is a per-type connector number. Each
    * (type, type_id) combination is unique across all connectors of a DRM
    * device.
    *
    * The (type, type_id) combination is not a stable identifier: the
    * type_id can change depending on the driver probe order.
    */
    u32 connector_type_id;

    /**
    * @connection: Status of the connector.
    *
    * See enum drm_connector_status.
    */
    u32 connection;
    /** @mm_width: Width of the connected sink in millimeters. */
    u32 mm_width;
    /** @mm_height: Height of the connected sink in millimeters. */
    u32 mm_height;
    /**
    * @subpixel: Subpixel order of the connected sink.
    *
    * See enum subpixel_order.
    */
    u32 subpixel;

    /** @pad: Padding, must be zero. */
    u32 pad;
};

enum drm_mode_connector_status {
    DRM_MODE_CONNECTOR_STATUS_CONNECTED = 1,
};

struct drm_mode_get_connector *drm_mode_get_connector(
    int card_fd,
    u32 connector_id,
    struct arena *arena
) {
    struct drm_mode_get_connector last_conn;
    struct arena temp_arena;
    struct drm_mode_get_connector *conn;

    do {
        temp_arena = *arena;

        conn = alloc(&temp_arena, sizeof(*conn), 1);
        conn->connector_id = connector_id;

        int error = ioctl(
            card_fd,
            ioctl_op(
                IOCTL_DIR_RDWR,
                sizeof(*conn),
                IOCTL_TYPE_DRM,
                DRM_IOCTL_MODE_GETCONNECTOR
            ),
            conn
        );

        if (error) {
            return 0;
        }

        last_conn = *conn;

        conn->props_ptr = alloc(
            &temp_arena,
            sizeof(*conn->props_ptr),
            conn->count_props
        );
        conn->prop_values_ptr = alloc(
            &temp_arena,
            sizeof(*conn->prop_values_ptr),
            conn->count_props
        );
        conn->modes_ptr = alloc(
            &temp_arena,
            sizeof(*conn->modes_ptr),
            conn->count_modes
        );
        conn->encoders_ptr = alloc(
            &temp_arena,
            sizeof(*conn->encoders_ptr),
            conn->count_encoders
        );

        error = ioctl(
            card_fd,
            ioctl_op(
                IOCTL_DIR_RDWR,
                sizeof(*conn),
                IOCTL_TYPE_DRM,
                DRM_IOCTL_MODE_GETCONNECTOR
            ),
            conn
        );

        if (error) {
            return 0;
        }
    } while (
        last_conn.count_props < conn->count_props ||
        last_conn.count_modes < conn->count_modes ||
        last_conn.count_encoders < conn->count_encoders
    );

    *arena = temp_arena;

    return conn;
}

struct drm_mode_get_encoder {
    u32 encoder_id;
    u32 encoder_type;

    u32 crtc_id; /**< Id of crtc */

    u32 possible_crtcs;
    u32 possible_clones;
};

struct drm_mode_get_encoder *drm_mode_get_encoder(
    int card_fd,
    u32 encoder_id,
    struct arena *arena
) {
    struct arena temp_arena = *arena;
    struct drm_mode_get_encoder *enc = alloc(&temp_arena, sizeof(*enc), 1);

    enc->encoder_id = encoder_id;

    int error = ioctl(
        card_fd,
        ioctl_op(
            IOCTL_DIR_RDWR,
            sizeof(*enc),
            IOCTL_TYPE_DRM,
            DRM_IOCTL_MODE_GETENCODER
        ),
        enc
    );

    if (error) {
        return 0;
    }

    *arena = temp_arena;

    return enc;
}

struct drm_mode_crtc {
    u32 *set_connectors_ptr;
    u32 count_connectors;

    u32 crtc_id; /**< Id */
    u32 fb_id; /**< Id of framebuffer */

    u32 x; /**< x Position on the framebuffer */
    u32 y; /**< y Position on the framebuffer */

    u32 gamma_size;
    u32 mode_valid;
    struct drm_mode_modeinfo mode;
};

struct drm_mode_crtc *drm_mode_get_crtc(
    int card_fd,
    u32 crtc_id,
    struct arena *arena
) {
    struct arena temp_arena = *arena;
    struct drm_mode_crtc *crtc = alloc(&temp_arena, sizeof(*crtc), 1);

    crtc->crtc_id = crtc_id;

    int error = ioctl(
        card_fd,
        ioctl_op(
            IOCTL_DIR_RDWR,
            sizeof(*crtc),
            IOCTL_TYPE_DRM,
            DRM_IOCTL_MODE_GETCRTC
        ),
        crtc
    );

    if (error) {
        return 0;
    }

    *arena = temp_arena;

    return crtc;
}

int drm_mode_set_crtc(
    int card_fd,
    u32 crtc_id,
    u32 fb_id,
    u32 conn_id,
    struct drm_mode_modeinfo mode
) {
    struct drm_mode_crtc crtc = {
        .set_connectors_ptr = &conn_id,
        .count_connectors = 1,
        .crtc_id = crtc_id,
        .fb_id = fb_id,
        .mode_valid = 1,
        .mode = mode,
    };

    int error = ioctl(
        card_fd,
        ioctl_op(
            IOCTL_DIR_RDWR,
            sizeof(crtc),
            IOCTL_TYPE_DRM,
            DRM_IOCTL_MODE_SETCRTC
        ),
        &crtc
    );

    if (error) {
        return -1;
    }

    return 0;
}

struct drm_mode_fb_cmd {
    u32 fb_id;
    u32 width;
    u32 height;
    u32 pitch;
    u32 bpp;
    u32 depth;
    /* driver specific handle */
    u32 handle;
};

struct drm_mode_create_dumb {
    u32 height;
    u32 width;
    u32 bpp;
    u32 flags;

    u32 handle;
    u32 pitch;
    u64 size;
};

struct drm_mode_map_dumb {
    /** Handle for the object being mapped. */
    u32 handle;
    u32 pad;
    /**
    * Fake offset to use for subsequent mmap call
    *
    * This is a fixed-size type for 32/64 compatibility.
    */
    u64 offset;
};

struct dumb_buffer {
    u32 fb_id;
    u32 width;
    u32 height;
    u32 stride;
    u32 *mem;
    u64 size;
};

struct dumb_buffer *create_dumb_buffer(
    int card_fd,
    u32 width,
    u32 height,
    struct arena *arena
) {
    struct drm_mode_create_dumb creq = {
        .width = width,
        .height = height,
        .bpp = 32,
    };

    int error = ioctl(
        card_fd,
        ioctl_op(
            IOCTL_DIR_RDWR,
            sizeof(creq),
            IOCTL_TYPE_DRM,
            DRM_IOCTL_MODE_CREATE_DUMB
        ),
        &creq
    );

    if (error) {
        return 0;
    }

    struct drm_mode_map_dumb mreq = { .handle = creq.handle };

    error = ioctl(
        card_fd,
        ioctl_op(
            IOCTL_DIR_RDWR,
            sizeof(mreq),
            IOCTL_TYPE_DRM,
            DRM_IOCTL_MODE_MAP_DUMB
        ),
        &mreq
    );

    if (error) {
        return 0;
    }

    struct drm_mode_fb_cmd fbreq = {
        .width = creq.width,
        .height = creq.height,
        .pitch = creq.pitch,
        .bpp = creq.bpp,
        .depth = 24,
        .handle = creq.handle,
    };

    error = ioctl(
        card_fd,
        ioctl_op(
            IOCTL_DIR_RDWR,
            sizeof(fbreq),
            IOCTL_TYPE_DRM,
            DRM_IOCTL_MODE_ADDFB
        ),
        &fbreq
    );

    if (error) {
        return 0;
    }

    u32 *mem = mmap(
        0,
        creq.size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        card_fd,
        mreq.offset
    );

    if (mem == 0) {
        return 0;
    }

    struct dumb_buffer *db = alloc(arena, sizeof(*db), 1);
    db->fb_id = fbreq.fb_id;
    db->width = creq.width;
    db->height = creq.height;
    db->stride = creq.pitch / sizeof(*db->mem);
    db->size = creq.size;
    db->mem = mem;

    return db;
}

const i64 ARENA_SIZE = 4096 * 2000;

int main(int argc, char **argv) {
    mount("", "/dev", "devtmpfs", 0, 0);
    int card_fd = open("/dev/dri/card0", O_RDWR, 0);

    char *mem = mmap(
        0,
        ARENA_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS,
        -1,
        0
    );

    struct arena arena = { .beg = mem, .end = mem + ARENA_SIZE };

    struct drm_mode_card_res *res = drm_mode_get_res(card_fd, &arena);

    struct drm_mode_get_connector *conn = 0;

    for (u32 i = 0; i < res->count_connectors; i++) {
        struct arena temp_arena = arena;
        u32 conn_id = res->connector_id_ptr[i];
        conn = drm_mode_get_connector(card_fd, conn_id, &temp_arena);

        if (
            conn == 0 ||
            conn->connection != DRM_MODE_CONNECTOR_STATUS_CONNECTED ||
            conn->count_modes == 0
        ) {
            conn = 0;
            continue;
        }

        arena = temp_arena;
        break;
    }

    if (conn == 0) {
        panic("failed to find connector");
    }

    struct drm_mode_crtc *crtc = 0;

    if (conn->encoder_id != 0) {
        struct arena temp_arena = arena;
        struct drm_mode_get_encoder *enc = drm_mode_get_encoder(
            card_fd,
            conn->encoder_id,
            &temp_arena
        );

        if (enc != 0 && enc->crtc_id != 0) {
            crtc = drm_mode_get_crtc(card_fd, enc->crtc_id, &temp_arena);

            if (crtc != 0) {
                arena = temp_arena;
            }
        }
    }

    if (crtc == 0) {
        for (u32 enc_idx = 0; enc_idx < conn->count_encoders; enc_idx++) {
            struct arena temp_arena = arena;
            u32 enc_id = conn->encoders_ptr[enc_idx];
            struct drm_mode_get_encoder *enc = drm_mode_get_encoder(
                card_fd,
                enc_id,
                &temp_arena
            );

            if (enc == 0) {
                continue;
            }

            for (u32 crtc_idx = 0; crtc_idx < res->count_crtcs; crtc_idx++) {
                if ((enc->possible_crtcs & (1 << crtc_idx)) == 0) {
                    continue;
                }

                u32 crtc_id = res->crtc_id_ptr[crtc_idx];

                crtc = drm_mode_get_crtc(card_fd, crtc_id, &temp_arena);

                if (crtc != 0) {
                    break;
                }
            }

            if (crtc != 0) {
                arena = temp_arena;
                break;
            }
        }
    }

    if (crtc == 0) {
        panic("failed to find crtc");
    }

    struct drm_mode_modeinfo mode = conn->modes_ptr[0];

    struct dumb_buffer *dbuf = create_dumb_buffer(
        card_fd,
        mode.hdisplay,
        mode.vdisplay,
        &arena
    );

    u32 offset = 0;
    for (;;) {
        for (u32 y = 0; y < dbuf->height; y++) {
            for (u32 x = 0; x < dbuf->width; x++) {
                u32 color = ((offset + y) & 0xff) | (((offset + x) & 0xff) << 8) | (
                    ((offset + x + y) & 0xff) << 16
                );
                dbuf->mem[y * dbuf->stride + x] = color;
            }
        }

        int error = drm_mode_set_crtc(
            card_fd,
            crtc->crtc_id,
            dbuf->fb_id,
            conn->connector_id,
            mode
        );

        offset += 1;
    }

    return 0;
}

void _start_c(int argc, char **argv) {
    exit(main(argc, argv));
}
