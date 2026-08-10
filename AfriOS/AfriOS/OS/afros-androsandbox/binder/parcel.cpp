/*
 * binder/parcel.cpp — Implementation of the Parcel class.
 *
 * A Parcel is Android's idiomatic wire format for binder transactions. This
 * file provides the read/write surface that mirrors android::Parcel from
 * the framework: integers, strings, blobs, IBinder references, and a few
 * small helpers (data size, position, reset). The on-the-wire format is
 * simple and self-describing enough to round-trip between sandbox
 * processes: every value is prefixed by a 4-byte type tag so that a
 * reader can resync after a malformed write.
 *
 * Lifecycle: a Parcel owns its backing buffer (capacity-grown by powers of
 * two); copies are deep. Reads mutate an internal cursor that is reset by
 * setDataPosition(0).
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <cstdlib>
#include <algorithm>

/* Type tags recorded on the wire so a reader can validate the layout. */
enum ParcelTag : uint32_t {
    TAG_INT32   = 0x49333200, /* "I32\0" */
    TAG_INT64   = 0x49363400, /* "I64\0" */
    TAG_STRING  = 0x53545200, /* "STR\0" */
    TAG_BLOB    = 0x424C4200, /* "BLB\0" */
    TAG_BINDER  = 0x424E4400, /* "BND\0" */
};

class Parcel {
public:
    Parcel() : data_(nullptr), capacity_(0), wpos_(0), rpos_(0) {
        reserve(64);
    }
    ~Parcel() { free(data_); }

    Parcel(const Parcel &o) : data_(nullptr), capacity_(0), wpos_(0), rpos_(0) {
        reserve(o.capacity_);
        if (o.data_ && o.wpos_) {
            memcpy(data_, o.data_, o.wpos_);
            wpos_ = o.wpos_;
        }
    }
    Parcel &operator=(const Parcel &o) {
        if (this != &o) {
            wpos_ = 0; rpos_ = 0;
            reserve(o.capacity_);
            if (o.data_ && o.wpos_) {
                memcpy(data_, o.data_, o.wpos_);
                wpos_ = o.wpos_;
            }
        }
        return *this;
    }

    /* ---------- write side ---------- */
    status_t writeInt32(int32_t v) {
        if (!ensure(sizeof(uint32_t) * 2)) return NO_MEMORY;
        appendTag(TAG_INT32);
        memcpy(data_ + wpos_, &v, sizeof(v));
        wpos_ += sizeof(v);
        return OK;
    }
    status_t writeInt64(int64_t v) {
        if (!ensure(sizeof(uint32_t) + sizeof(v))) return NO_MEMORY;
        appendTag(TAG_INT64);
        memcpy(data_ + wpos_, &v, sizeof(v));
        wpos_ += sizeof(v);
        return OK;
    }
    status_t writeString(const String8 &s)   { return writeString(s.c_str()); }
    status_t writeString(const char *s) {
        if (!s) s = "";
        size_t len = strlen(s);
        if (!ensure(sizeof(uint32_t) * 2 + len + 1)) return NO_MEMORY;
        appendTag(TAG_STRING);
        uint32_t l = (uint32_t)len;
        memcpy(data_ + wpos_, &l, sizeof(l));
        wpos_ += sizeof(l);
        memcpy(data_ + wpos_, s, len);
        wpos_ += len;
        data_[wpos_++] = 0; /* NUL terminator */
        return OK;
    }
    status_t writeBlob(const void *buf, size_t len) {
        if (!ensure(sizeof(uint32_t) * 2 + len)) return NO_MEMORY;
        appendTag(TAG_BLOB);
        uint32_t l = (uint32_t)len;
        memcpy(data_ + wpos_, &l, sizeof(l));
        wpos_ += sizeof(l);
        if (len) { memcpy(data_ + wpos_, buf, len); wpos_ += len; }
        return OK;
    }
    status_t writeStrongBinder(binder_handle_t h) {
        if (!ensure(sizeof(uint32_t) * 2)) return NO_MEMORY;
        appendTag(TAG_BINDER);
        memcpy(data_ + wpos_, &h, sizeof(h));
        wpos_ += sizeof(h);
        return OK;
    }

    /* ---------- read side ---------- */
    status_t readInt32(int32_t *out) {
        if (!expectTag(TAG_INT32)) return BAD_VALUE;
        if (rpos_ + sizeof(int32_t) > wpos_) return NOT_ENOUGH_DATA;
        memcpy(out, data_ + rpos_, sizeof(int32_t));
        rpos_ += sizeof(int32_t);
        return OK;
    }
    status_t readInt64(int64_t *out) {
        if (!expectTag(TAG_INT64)) return BAD_VALUE;
        if (rpos_ + sizeof(int64_t) > wpos_) return NOT_ENOUGH_DATA;
        memcpy(out, data_ + rpos_, sizeof(int64_t));
        rpos_ += sizeof(int64_t);
        return OK;
    }
    /* Reads into a caller-provided buffer; returns the length, or -errno. */
    ssize_t readString(char *out, size_t out_len) {
        if (!expectTag(TAG_STRING)) return BAD_VALUE;
        uint32_t l;
        if (rpos_ + sizeof(l) > wpos_) return NOT_ENOUGH_DATA;
        memcpy(&l, data_ + rpos_, sizeof(l)); rpos_ += sizeof(l);
        if (rpos_ + l + 1 > wpos_) return NOT_ENOUGH_DATA;
        if (out && out_len) {
            size_t cp = std::min<size_t>(l, out_len - 1);
            memcpy(out, data_ + rpos_, cp);
            out[cp] = 0;
        }
        rpos_ += l + 1;
        return (ssize_t)l;
    }
    ssize_t readBlob(void *out, size_t out_len) {
        if (!expectTag(TAG_BLOB)) return BAD_VALUE;
        uint32_t l;
        if (rpos_ + sizeof(l) > wpos_) return NOT_ENOUGH_DATA;
        memcpy(&l, data_ + rpos_, sizeof(l)); rpos_ += sizeof(l);
        if (rpos_ + l > wpos_) return NOT_ENOUGH_DATA;
        if (out && out_len) {
            size_t cp = std::min<size_t>(l, out_len);
            memcpy(out, data_ + rpos_, cp);
        }
        rpos_ += l;
        return (ssize_t)l;
    }
    status_t readStrongBinder(binder_handle_t *out) {
        if (!expectTag(TAG_BINDER)) return BAD_VALUE;
        if (rpos_ + sizeof(binder_handle_t) > wpos_) return NOT_ENOUGH_DATA;
        memcpy(out, data_ + rpos_, sizeof(binder_handle_t));
        rpos_ += sizeof(binder_handle_t);
        return OK;
    }

    /* ---------- accessors ---------- */
    const void *data() const { return data_; }
    size_t      dataSize() const { return wpos_; }
    size_t      dataPosition() const { return rpos_; }
    void        setDataPosition(size_t p) { rpos_ = std::min(p, wpos_); }
    void        clear() { wpos_ = 0; rpos_ = 0; }

private:
    bool ensure(size_t add) {
        size_t need = wpos_ + add;
        if (need <= capacity_) return true;
        size_t newcap = capacity_ ? capacity_ : 64;
        while (newcap < need) newcap *= 2;
        return reserve(newcap);
    }
    bool reserve(size_t newcap) {
        if (newcap <= capacity_) return true;
        uint8_t *p = (uint8_t *)realloc(data_, newcap);
        if (!p) return false;
        data_ = p;
        capacity_ = newcap;
        return true;
    }
    void appendTag(uint32_t t) {
        memcpy(data_ + wpos_, &t, sizeof(t));
        wpos_ += sizeof(t);
    }
    bool expectTag(uint32_t expected) {
        if (rpos_ + sizeof(uint32_t) > wpos_) return false;
        uint32_t got;
        memcpy(&got, data_ + rpos_, sizeof(got));
        if (got != expected) return false;
        rpos_ += sizeof(uint32_t);
        return true;
    }

    uint8_t *data_;
    size_t   capacity_;
    size_t   wpos_;
    size_t   rpos_;
};

/* ---------- C-callable wrappers used by the rest of the sandbox ---------- */
extern "C" {

typedef Parcel *ParcelHandle;

ParcelHandle ParcelCreate()                                  { return new Parcel(); }
void          ParcelDestroy(ParcelHandle p)                  { delete p; }
status_t      ParcelWriteInt32(ParcelHandle p, int32_t v)    { return p->writeInt32(v); }
status_t      ParcelWriteString(ParcelHandle p, const char *s) { return p->writeString(s); }
status_t      ParcelWriteBlob(ParcelHandle p, const void *b, size_t l) { return p->writeBlob(b, l); }
status_t      ParcelWriteBinder(ParcelHandle p, binder_handle_t h)     { return p->writeStrongBinder(h); }

status_t      ParcelReadInt32(ParcelHandle p, int32_t *out)  { return p->readInt32(out); }
ssize_t       ParcelReadString(ParcelHandle p, char *out, size_t l) { return p->readString(out, l); }
ssize_t       ParcelReadBlob(ParcelHandle p, void *out, size_t l)   { return p->readBlob(out, l); }
status_t      ParcelReadBinder(ParcelHandle p, binder_handle_t *h)  { return p->readStrongBinder(h); }

size_t        ParcelDataSize(ParcelHandle p)                 { return p->dataSize(); }
const void   *ParcelData(ParcelHandle p)                     { return p->data(); }
void          ParcelSetPosition(ParcelHandle p, size_t pos)  { p->setDataPosition(pos); }

} /* extern "C" */
