// =============================================================================
//  PetPal - ByteBuffer.h
//  Minimal little-endian binary writer/reader for save serialization. Writing
//  always appends; reading tracks a cursor and reports overruns so a truncated
//  or corrupt save fails safely instead of reading garbage.
// =============================================================================
#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace petpal {

class ByteWriter {
public:
    template <typename T>
    void put(T v) {
        static_assert(std::is_trivially_copyable<T>::value, "put<T> needs a POD");
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        buf_.insert(buf_.end(), p, p + sizeof(T));
    }

    void putU8 (uint8_t v)  { put(v); }
    void putU16(uint16_t v) { put(v); }
    void putU32(uint32_t v) { put(v); }
    void putU64(uint64_t v) { put(v); }
    void putF32(float v)    { put(v); }

    // Length-prefixed (u16) UTF-8 string.
    void putString(const std::string& s) {
        const uint16_t len = static_cast<uint16_t>(s.size() > 0xFFFF ? 0xFFFF : s.size());
        putU16(len);
        buf_.insert(buf_.end(), s.begin(), s.begin() + len);
    }

    void putBytes(const void* data, size_t len) {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        buf_.insert(buf_.end(), p, p + len);
    }

    const std::vector<uint8_t>& bytes() const { return buf_; }
    std::vector<uint8_t>&       bytes()       { return buf_; }
    size_t size() const { return buf_.size(); }

private:
    std::vector<uint8_t> buf_;
};

class ByteReader {
public:
    ByteReader(const uint8_t* data, size_t len) : data_(data), len_(len) {}

    bool ok() const { return ok_; }
    size_t remaining() const { return pos_ <= len_ ? len_ - pos_ : 0; }

    template <typename T>
    T get() {
        static_assert(std::is_trivially_copyable<T>::value, "get<T> needs a POD");
        T v{};
        if (pos_ + sizeof(T) > len_) { ok_ = false; return v; }
        std::memcpy(&v, data_ + pos_, sizeof(T));
        pos_ += sizeof(T);
        return v;
    }

    uint8_t  getU8 () { return get<uint8_t>(); }
    uint16_t getU16() { return get<uint16_t>(); }
    uint32_t getU32() { return get<uint32_t>(); }
    uint64_t getU64() { return get<uint64_t>(); }
    float    getF32() { return get<float>(); }

    std::string getString() {
        const uint16_t len = getU16();
        if (!ok_ || pos_ + len > len_) { ok_ = false; return {}; }
        std::string s(reinterpret_cast<const char*>(data_ + pos_), len);
        pos_ += len;
        return s;
    }

    void skip(size_t n) { if (pos_ + n > len_) ok_ = false; else pos_ += n; }

private:
    const uint8_t* data_;
    size_t         len_;
    size_t         pos_ = 0;
    bool           ok_  = true;
};

} // namespace petpal
