// Copyright 2016 The Fuchsia Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <magenta/types.h>
#include <sys/types.h>

#include <runtime/mutex.h>
#include <system/atomic.h>

typedef struct mxio mxio_t;

// MXIO provides open/close/read/write io over various transports
// via the mxio_t interface abstraction.
//
// The PIPE protocol uses message ports as simple, no-flow-control
// io pipes with a maximum message size of MX_PIPE_SIZE.
//
// The REMOTEIO protocol uses message ports to implement simple
// synchronous remoting of read/write/close operations.
//
// The NULL protocol absorbs writes and is never readable.

typedef struct mxio_ops {
    ssize_t (*read)(mxio_t* io, void* data, size_t len);
    ssize_t (*write)(mxio_t* io, const void* data, size_t len);
    off_t (*seek)(mxio_t* io, off_t offset, int whence);
    mx_status_t (*misc)(mxio_t* io, uint32_t op, uint32_t maxreply, void* data, size_t len);
    mx_status_t (*close)(mxio_t* io);
    mx_status_t (*open)(mxio_t* io, const char* path, int32_t flags, mxio_t** out);
    mx_status_t (*clone)(mxio_t* io, mx_handle_t* out_handles, uint32_t* out_types);
    mx_status_t (*wait)(mxio_t* io, uint32_t events, uint32_t* pending, mx_time_t timeout);
    ssize_t (*ioctl)(mxio_t* io, uint32_t op, const void* in_buf, size_t in_len, void* out_buf, size_t out_len);
} mxio_ops_t;

typedef struct mxio {
    mxio_ops_t* ops;
    uint32_t magic;
    int32_t refcount;
} mxio_t;

// Lifecycle notes:
//
// Upon creation, mxio objects have a refcount of 1.
// mxio_acquire() and mxio_release() are used to upref
// and downref, respectively.  Upon downref to 0,
// mxio_free() is called, which poisons the object and
// free()s it.
//
// The close hook must be called before free and should
// only be called once.  In normal use, mxio objects are
// accessed through the mxio_fdtab, and when close is
// called they are removed from the fdtab and the reference
// that the fdtab itself is holding is released, at which
// point they will be free()'d unless somebody is holding
// a ref due to an ongoing io transaction, which will
// certainly fail doe to underlying handles being closed
// at which point a downref will happen and destruction
// will follow.

#define MXIO_MAGIC 0x4f49584d // MXIO

static inline ssize_t mx_read(mxio_t* io, void* data, size_t len) {
    return io->ops->read(io, data, len);
}
static inline ssize_t mx_write(mxio_t* io, const void* data, size_t len) {
    return io->ops->write(io, data, len);
}
static inline off_t mx_seek(mxio_t* io, off_t offset, int whence) {
    return io->ops->seek(io, offset, whence);
}
static inline mx_status_t mx_misc(mxio_t* io, uint32_t op, uint32_t maxreply, void* data, size_t len) {
    return io->ops->misc(io, op, maxreply, data, len);
}
static inline mx_status_t mx_close(mxio_t* io) {
    return io->ops->close(io);
}
static inline mx_status_t mx_open(mxio_t* io, const char* path, int32_t flags, mxio_t** out) {
    return io->ops->open(io, path, flags, out);
}

// wraps a message port with an mxio_t using simple io
mxio_t* mxio_pipe_create(mx_handle_t h);

// creates a message port and pair of simple io mxio_t's
int mxio_pipe_pair(mxio_t** a, mxio_t** b);

// create a mxio (if possible) from type and handles
mx_status_t mxio_from_handles(uint32_t type, mx_handle_t* handles, int hcount, mxio_t** out);


void mxio_free(mxio_t* io);

static inline void mxio_acquire(mxio_t* io) {
    atomic_add_int32(&io->refcount, 1);
}

static inline void mxio_release(mxio_t* io) {
    if (atomic_add_int32(&io->refcount, -1) == 1) {
        mxio_free(io);
    }
}

// unsupported / do-nothing hooks shared by implementations
ssize_t mxio_default_read(mxio_t* io, void* _data, size_t len);
ssize_t mxio_default_write(mxio_t* io, const void* _data, size_t len);
off_t mxio_default_seek(mxio_t* io, off_t offset, int whence);
mx_status_t mxio_default_misc(mxio_t* io, uint32_t op, uint32_t arg, void* data, size_t len);
mx_status_t mxio_default_close(mxio_t* io);
mx_status_t mxio_default_open(mxio_t* io, const char* path, int32_t flags, mxio_t** out);
mx_handle_t mxio_default_clone(mxio_t* io, mx_handle_t* handles, uint32_t* types);
mx_status_t mxio_default_wait(mxio_t* io, uint32_t events, uint32_t* pending, mx_time_t timeout);
ssize_t mxio_default_ioctl(mxio_t* io, uint32_t op, const void* in_buf, size_t in_len, void* out_buf, size_t out_len);