//////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024  Rohit Jairaj Singh (rohit@singh.org.in)          //
//                                                                      //
// This program is free software: you can redistribute it and/or modify //
// it under the terms of the GNU General Public License as published by //
// the Free Software Foundation, either version 3 of the License, or    //
// (at your option) any later version.                                  //
//                                                                      //
// This program is distributed in the hope that it will be useful,      //
// but WITHOUT ANY WARRANTY; without even the implied warranty of       //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        //
// GNU General Public License for more details.                         //
//                                                                      //
// You should have received a copy of the GNU General Public License    //
// along with this program.  If not, see <https://www.gnu.org/licenses/ //
//////////////////////////////////////////////////////////////////////////

#pragma once
#include <charconv>
#include <concepts>
#include <type_traits>
#include <exception>
#include <string>
#include <stdint.h>
#include <bit>
#include <stdexcept>
#include <filesystem>
#include <fstream>

namespace rohit {

template <typename T>
constexpr T byteswap(const T &val) {
#if __cpp_lib_byteswap
        return std::byteswap(val);
#else
        if constexpr (sizeof(val) == 2) {
            if constexpr (std::is_signed_v<T> && !std::is_same_v<T, bool>) {
                const auto retval1 = static_cast<std::make_unsigned_t<T>>(val) >> 8;
                const auto retval2 = static_cast<std::make_unsigned_t<T>>(val) << 8;
                return static_cast<T>(retval1 | retval2);
            } else {
                return (val >> 8) | (val << 8);
            }
        } else if constexpr (sizeof(val) == 4) {
            T retval = ((val >> 8) & 0x0000FF00) | ((val << 8) & 0x00FF0000);
            if constexpr (std::is_signed_v<T> && !std::is_same_v<T, bool>) {
                const auto retval1 = static_cast<std::make_unsigned_t<T>>(val) >> 24;
                const auto retval2 = static_cast<std::make_unsigned_t<T>>(val) << 24;
                return static_cast<T>(retval1 | retval2 | retval);
            } else return (val >> 24) | (val << 24) | retval;
        } else if constexpr (sizeof(val) == 8) {
            T retval =  ((val >> 40) & 0x000000000000FF00ULL) | 
                                    ((val >> 24) & 0x0000000000FF0000ULL) | 
                                    ((val >> 8)  & 0x00000000FF000000ULL) | 
                                    ((val << 8)  & 0x000000FF00000000ULL) | 
                                    ((val << 24) & 0x0000FF0000000000ULL) | 
                                    ((val << 40) & 0x00FF000000000000ULL);
            if constexpr (std::is_signed_v<T> && !std::is_same_v<T, bool>) {
                const auto retval1 = static_cast<std::make_unsigned_t<T>>(val) >> 56;
                const auto retval2 = static_cast<std::make_unsigned_t<T>>(val) << 56;
                return static_cast<T>(retval1 | retval2 | retval);
            } else return (val >> 56) | (val << 56) | retval;
        } else static_assert(false, "Unsupported type");
#endif
}

template <std::endian source, std::endian destination, typename T>
constexpr T ChangeEndian(const T &val) {
    if constexpr (source == destination || sizeof(val) == 1) return val;
    else return byteswap(val);
}

namespace exception {
class StreamOverflowException : public std::exception {
public:
    const char *what() const noexcept override {
        return "Stream Overflow";
    }
}; // class StreamOverflowException

class StreamUnderflowException : public std::exception {
public:
    const char *what() const noexcept override {
        return "Stream Underflow";
    }
}; // class StreamUnderflowException

class MemoryAllocationException : public std::exception {
public:
    const char *what() const noexcept override {
        return "Stream unable to allocate memory";
    }
}; // class MemoryAllocationException

} // namespace exception

template <size_t size>
consteval size_t Hash(const char (&str)[size]) {
    size_t ret = 100000000003;
    for(auto ch: str) {
        ret = ((ret << 9) + ret) ^ static_cast<size_t>(ch);
    }
    return ret;
}

constexpr size_t Hash(const char *str) {
    size_t ret = 100000000003;
    while(*str) {
        ret = ((ret << 9) + ret) ^ static_cast<size_t>(*str++);
    }
    return ret;
}

constexpr size_t Hash(const std::string &str) {
    size_t ret = 100000000003;
    for(auto ch: str) {
        ret = ((ret << 9) + ret) ^ static_cast<size_t>(ch);
    }
    return ret;
}

constexpr size_t Hash(const std::string_view &str) {
    size_t ret = 100000000003;
    for(auto ch: str) {
        ret = ((ret << 9) + ret) ^ static_cast<size_t>(ch);
    }
    return ret;
}

class Stream {
protected:
    friend class FixedBuffer;
    mutable uint8_t *_curr;
    uint8_t * _end;

    void CheckOverflow() const { if (_curr >= _end) throw exception::StreamOverflowException { }; }
    void CheckOverflow(size_t len) const { if (_curr + len > _end) throw exception::StreamOverflowException { }; }

    Stream() : _curr { nullptr }, _end { nullptr } { }
public:
    Stream(auto *_begin, auto *_end) : _curr { reinterpret_cast<uint8_t *>(_begin) }, _end { reinterpret_cast<uint8_t *>(_end) } { }
    Stream(auto *_begin, size_t size) : _curr { reinterpret_cast<uint8_t *>(_begin) }, _end { _curr + size } { }
    Stream(Stream &&stream) : _curr { stream._curr }, _end { stream._end } { stream._curr = stream._end = nullptr; }
    Stream(std::string &string) : _curr { reinterpret_cast<uint8_t *>(string.data()) }, _end { _curr + string.size() } { }
    Stream(const Stream &stream) : _curr { stream._curr }, _end { stream._end } { }
    virtual ~Stream() = default;

    Stream GetSimpleStream() { return Stream { _curr, _end }; }
    const Stream GetSimpleStream() const { return Stream { _curr, _end }; }
    const Stream GetSimpleConstStream() const { return Stream { _curr, _end }; }

    template <size_t _size>
    bool operator==(const auto (&data)[_size + 1]) const {
        if (RemainingBuffer() < _size) return false;
        return std::equal(std::begin(data), std::end(data), _curr);
    }

    /*! IMPORTANT: text must not be null terminated */
    bool operator==(const std::string_view &text) const {
        if (RemainingBuffer() < text.size()) return false;
        return std::equal(std::begin(text), std::end(text), _curr);
    }

    Stream &operator=(const Stream &stream) { _curr = stream._curr; return *this; }
    const Stream &operator=(const Stream &stream) const { _curr = stream._curr; return *this; }
    virtual Stream operator+(size_t len) { _curr += len; return *this; }
    virtual const Stream operator+(size_t len) const { _curr += len; return *this; }
    virtual Stream &operator+=(size_t len) { _curr += len; return *this; }
    virtual const Stream &operator+=(size_t len) const { _curr += len; return *this; }

    inline uint8_t &operator*() { return *_curr; }
    inline uint8_t operator*() const { return *_curr; }
    inline uint8_t &at_unchecked() { return *_curr; }
    inline uint8_t at_unchecked() const { return *_curr; }
    virtual inline Stream &operator++() { ++_curr; return *this; }
    virtual inline Stream &operator--() { --_curr; return *this;}
    virtual inline const Stream &operator++() const { ++_curr; return *this; }
    virtual inline const Stream &operator--() const { --_curr; return *this;}
    inline void push(const auto ch) { operator*() = static_cast<uint8_t>(ch); operator++(); }

    virtual inline uint8_t *GetCurrAndIncrease(const size_t len) { auto temp = _curr; _curr += len; return temp; }
    virtual inline const uint8_t *GetCurrAndIncrease(const size_t len) const { auto temp = _curr; _curr += len; return temp; }

    size_t GetSizeFrom(const auto *start) const { return static_cast<size_t>(_curr - reinterpret_cast<const uint8_t *>(start)); }

    #if defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Weffc++"
    #endif
    virtual inline uint8_t *operator++(int) { uint8_t *temp = _curr; ++_curr; return temp; };
    virtual inline uint8_t *operator--(int) { uint8_t *temp = _curr; --_curr; return temp; };
    virtual inline const uint8_t *operator++(int) const { uint8_t *temp = _curr; ++_curr; return temp; };
    virtual inline const uint8_t *operator--(int) const { uint8_t *temp = _curr; --_curr; return temp; };
    #if defined(__GNUC__)
    #pragma GCC diagnostic pop
    #endif

    const uint8_t *end() const { return _end; }
    const uint8_t *curr() const { return _curr; }

    auto &end() { return _end; }
    auto &curr() { return _curr; }

    size_t RemainingBuffer() const { return static_cast<size_t>(_end - _curr); }

    bool full() const { return _curr == _end; }

    template <typename T>
    bool CheckCapacity() const { return RemainingBuffer() >= sizeof(T); }
    bool CheckCapacity(const size_t size) const { return RemainingBuffer() >= size; }

    void UpdateCurr(uint8_t *incurr) const { this->_curr = incurr; }

    auto GetRawCurrentBuffer() { return std::make_pair(_curr, RemainingBuffer()); }

    virtual inline void Reserve(const size_t len) { CheckOverflow(len); }
    inline void Reserve(const auto *_begin, const auto *_end) { 
        const size_t len = reinterpret_cast<const char *>(_end) - reinterpret_cast<const char *>(_begin);
        Reserve(len);
    }
    inline void Copy(const std::string_view &source) { Reserve(source.size()); _curr = std::copy(std::begin(source), std::end(source), _curr); }
    inline void Copy(const std::string &source) { Reserve(source.size()); _curr = std::copy(std::begin(source), std::end(source), _curr); }
    inline void Copy(const Stream &source) { Reserve(source.RemainingBuffer()); _curr = std::copy(source.curr(), source.end(), _curr); }
    inline void Copy(const auto *begin, const auto *end) { Reserve(begin, end); _curr = std::copy(reinterpret_cast<const uint8_t *>(begin), reinterpret_cast<const uint8_t *>(end), _curr); }
    inline void Copy(const auto *begin, size_t size) { Reserve(size); _curr = std::copy(reinterpret_cast<const uint8_t *>(begin), reinterpret_cast<const uint8_t *>(begin) + size, _curr); }

    template <typename ValueType>
    inline void Copy(const ValueType &value) {
        if constexpr (std::is_same_v<ValueType, char>) {
            Reserve(sizeof(value));
            *_curr++ = value;
        } else if constexpr (std::is_integral_v<ValueType>) {
            char buffer[std::numeric_limits<ValueType>::digits10 + 3] { 0 };
            auto result = std::to_chars(std::begin(buffer), std::end(buffer), value);
            Copy(buffer, result.ptr);
        } else if constexpr (std::is_array_v<ValueType>) {
            if constexpr (sizeof(value[0]) == 1) {
                constexpr auto array_size = sizeof(value)/sizeof(value[0]);
                if constexpr (array_size >= 1) {
                    if (value[array_size - 1] == '\0')
                        Copy(std::begin(value), array_size - 1);
                    else Copy(std::begin(value), std::end(value));
                }
            } else static_assert(false, "Unsupported type");
        } else if constexpr (std::is_same_v<ValueType, std::string>) {
            Copy(value);
        } else if constexpr (std::is_same_v<ValueType, std::string_view>) {
            Copy(value);
        }else static_assert(false, "Unsupported type");
    }

    template<typename... ValueType> 
    inline void Write(const ValueType& ...value) {
        ((Copy(value)), ...);
    }
}; // class Stream

class FullStream : public Stream {
protected:
    friend class FixedBuffer;
    uint8_t * _begin;

    void CheckUnderflow() const { if (_curr == _begin) throw exception::StreamUnderflowException { }; }

    FullStream() : Stream { }, _begin { nullptr } { }
public:
    FullStream(auto *_begin, auto *_end) : Stream {reinterpret_cast<uint8_t *>(_begin), reinterpret_cast<uint8_t *>(_end)}, _begin { reinterpret_cast<uint8_t *>(_begin) } { }
    FullStream(auto *_begin, auto *_end, auto *_curr) : Stream {reinterpret_cast<uint8_t *>(_curr), reinterpret_cast<uint8_t *>(_end)}, _begin { reinterpret_cast<uint8_t *>(_begin) } { }
    FullStream(auto *_begin, size_t size) :  Stream {reinterpret_cast<uint8_t *>(_begin), size}, _begin { reinterpret_cast<uint8_t *>(_begin) } { }
    FullStream(FullStream &&stream) : Stream { std::move(stream) }, _begin { stream._begin } { stream._begin = nullptr; }
    FullStream(const FullStream &stream) : Stream { stream }, _begin { stream._begin } { }

    FullStream &operator=(const FullStream &stream) { _curr = stream._curr; return *this; }

    const auto begin() const { return _begin; }
    auto begin() { return _begin; }

    auto CurrentOffset() const { return static_cast<size_t>(_curr - _begin); }
    auto Capacity() const { return static_cast<size_t>(_end - _begin); }

    uint8_t *Move() {
        auto temp = _begin;
        _begin = _end = _curr = nullptr;
        return temp;
    }

    bool IsNull() const { return _begin == nullptr; }
    auto GetRawFullBuffer() { return std::make_pair(_begin, Capacity()); }
    void Reset() const { _curr = _begin; }
    bool IsEmpty() const { return _curr == _begin; }

    void WriteToFileTillOffset(const std::filesystem::path &path) {
        std::ofstream filestream { path, std::ios::binary };
        filestream.write(reinterpret_cast<const char *>(_begin), CurrentOffset());
        filestream.close();
    }
    void WriteToFileComplete(const std::filesystem::path &path) {
        std::ofstream filestream { path, std::ios::binary };
        filestream.write(reinterpret_cast<const char *>(_begin), Capacity());
        filestream.close();
    }
};

class StreamAutoFree : public FullStream {
public:
    using FullStream::FullStream;
    ~StreamAutoFree() { free(_begin); }
};

class FullStreamLimitChecked : public FullStream {
public:
    using FullStream::FullStream;
    #if defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Weffc++"
    #endif
    inline Stream &operator--() override { CheckUnderflow(); --_curr; return *this;}
    inline const Stream &operator--() const override { CheckUnderflow(); --_curr; return *this;}
    inline const uint8_t *operator--(int) const override { CheckUnderflow(); const uint8_t *temp = _curr; --_curr; return temp; };

    Stream operator+(size_t len) override { _curr += len; CheckOverflow(); return *this; }
    const Stream operator+(size_t len) const override { _curr += len; CheckOverflow(); return *this; }
    Stream &operator+=(size_t len) override { _curr += len; CheckOverflow(); return *this; }
    const Stream &operator+=(size_t len) const override { _curr += len; CheckOverflow(); return *this; }
    inline Stream &operator++() override { ++_curr; CheckOverflow(); return *this; }
    inline const Stream &operator++() const override { ++_curr; CheckOverflow(); return *this; }
    inline const uint8_t *GetCurrAndIncrease(const size_t len) const override { auto temp = _curr; CheckOverflow(); _curr += len; return temp; }
    #if defined(__GNUC__)
    #pragma GCC diagnostic pop
    #endif
};

class FullStreamAutoAlloc : public FullStream {
    inline void CheckResize() {
        if (_curr == _end) {
            auto curr_index = CurrentOffset();
            auto new_capacity = Capacity() * 2;
            _begin = reinterpret_cast<uint8_t *>(realloc(reinterpret_cast<void *>(_begin), new_capacity));
            _end = _begin + new_capacity;
            _curr = _begin + curr_index;
        }
    }

    inline void CheckResize(const size_t len) {
        if (_curr + len > _end) {
            auto curr_index = CurrentOffset();
            auto new_capacity = Capacity() * 2;
            while(curr_index + len > new_capacity) new_capacity += Capacity();
            _begin = reinterpret_cast<uint8_t *>(realloc(reinterpret_cast<void *>(_begin), new_capacity));
            if (_begin == nullptr) throw exception::MemoryAllocationException { };
            _end = _begin + new_capacity;
            _curr = _begin + curr_index;
        }
    }

public:
    using FullStream::FullStream;
    FullStreamAutoAlloc(const size_t size) : FullStream { reinterpret_cast<uint8_t *>(malloc(size)), size } { if (_begin == nullptr) throw exception::MemoryAllocationException { }; }
    FullStreamAutoAlloc() : FullStream { } { }
    ~FullStreamAutoAlloc() { free(_begin); }

    #if defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Weffc++"
    #endif
    inline Stream &operator--() override { CheckUnderflow(); --_curr; return *this;}
    inline const Stream &operator--() const override { CheckUnderflow(); --_curr; return *this;}
    inline Stream &operator++() override { CheckResize(); ++_curr; return *this; }
    inline const Stream &operator++() const override { CheckOverflow(); ++_curr; return *this; }
    inline uint8_t *operator++(int) override { CheckResize(); uint8_t *temp = _curr; ++_curr; return temp; };
    inline const uint8_t *operator++(int) const override { CheckOverflow(); uint8_t *temp = _curr; ++_curr; return temp; };
    inline Stream operator+(size_t len) override { CheckResize(len); _curr += len; return *this; }
    inline  const Stream operator+(size_t len) const override { _curr += len; CheckOverflow(); return *this; }
    inline Stream &operator+=(size_t len) override { CheckResize(len); _curr += len; return *this; }
    inline const Stream &operator+=(size_t len) const override { _curr += len; CheckOverflow(); return *this; }
    #if defined(__GNUC__)
    #pragma GCC diagnostic pop
    #endif


    inline void Reserve(const size_t len) override { CheckResize(len); }

    inline uint8_t *GetCurrAndIncrease(const size_t len) override { CheckResize(len); auto temp = _curr; _curr += len; return temp; }
    inline const uint8_t *GetCurrAndIncrease(const size_t len) const override { auto temp = _curr; _curr += len; CheckOverflow(); return temp; }

    auto ReturnOldAndAlloc(const size_t size) { 
        FullStream stream { _begin, _end, _curr };
        _begin = reinterpret_cast<uint8_t *>(malloc(size));
        if (_begin == nullptr) throw exception::MemoryAllocationException { }; 
        _curr = _begin;
        _end = _begin + size;
        return stream;
    }
};

struct streamlimit_t {
    size_t MinReadBuffer { 1024 };
    size_t MaxReadBuffer { 8192 };
};

class FullStreamAutoAllocLimits : public FullStream {
    const streamlimit_t *limits { nullptr };

    inline void CheckResize() {
        if (_curr == _end) {
            auto curr_index = CurrentOffset();
            auto new_capacity = Capacity() * 2;
            if (new_capacity > limits->MaxReadBuffer) throw exception::StreamOverflowException { };
            _begin = reinterpret_cast<uint8_t *>(realloc(reinterpret_cast<void *>(_begin), new_capacity));
            if (_begin == nullptr) throw exception::MemoryAllocationException { };
            _end = _begin + new_capacity;
            _curr = _begin + curr_index;
        }
    }

    inline void CheckResize(const size_t len) {
        if (_curr + len > _end) {
            auto curr_index = CurrentOffset();
            auto new_capacity = Capacity() * 2;
            while(curr_index + len > new_capacity) new_capacity += Capacity();
            if (new_capacity > limits->MaxReadBuffer) throw exception::StreamOverflowException { };
            _begin = reinterpret_cast<uint8_t *>(realloc(reinterpret_cast<void *>(_begin), new_capacity));
            if (_begin == nullptr) throw exception::MemoryAllocationException { };
            _end = _begin + new_capacity;
            _curr = _begin + curr_index;
        }
    }

    inline void SetMinBuffer() {
        auto current_buffer_size = static_cast<size_t>(_end - _begin);
        if (current_buffer_size < limits->MinReadBuffer) {
            auto curr_index = CurrentOffset();
            _begin = reinterpret_cast<uint8_t *>(realloc(reinterpret_cast<void *>(_begin), limits->MinReadBuffer));
            if (_begin == nullptr) throw exception::MemoryAllocationException { };
            _end = _begin + limits->MinReadBuffer;
            _curr = _begin + curr_index;
        }
    }

public:
    using FullStream::FullStream;
    FullStreamAutoAllocLimits(const streamlimit_t *limits) : FullStream { reinterpret_cast<uint8_t *>(malloc(limits->MinReadBuffer)), limits->MinReadBuffer }, limits { limits } { if (_begin == nullptr) throw exception::MemoryAllocationException { }; }
    FullStreamAutoAllocLimits() : FullStream { } { }
    ~FullStreamAutoAllocLimits() { free(_begin); }
    FullStreamAutoAllocLimits(const FullStreamAutoAllocLimits &) = delete;
    FullStreamAutoAllocLimits &operator=(const FullStreamAutoAllocLimits &) = delete;

    #if defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Weffc++"
    #endif
    inline Stream &operator--() override { CheckUnderflow(); --_curr; return *this;}
    inline const Stream &operator--() const override { CheckUnderflow(); --_curr; return *this;}
    inline Stream &operator++() override { CheckResize(); ++_curr; return *this; }
    inline const Stream &operator++() const override { CheckOverflow(); ++_curr; return *this; }
    inline uint8_t *operator++(int) override { CheckResize(); uint8_t *temp = _curr; ++_curr; return temp; };
    inline const uint8_t *operator++(int) const override { CheckOverflow(); uint8_t *temp = _curr; ++_curr; return temp; };
    inline Stream operator+(size_t len) override { CheckResize(len); _curr += len; return *this; }
    inline  const Stream operator+(size_t len) const override { _curr += len; CheckOverflow(); return *this; }
    inline Stream &operator+=(size_t len) override { CheckResize(len); _curr += len; return *this; }
    inline const Stream &operator+=(size_t len) const override { _curr += len; CheckOverflow(); return *this; }
    #if defined(__GNUC__)
    #pragma GCC diagnostic pop
    #endif

    inline void Reserve(const size_t len) override { CheckResize(len); }

    inline uint8_t *GetCurrAndIncrease(const size_t len) override { CheckResize(len); auto temp = _curr; _curr += len; return temp; }
    inline const uint8_t *GetCurrAndIncrease(const size_t len) const override { auto temp = _curr; _curr += len; CheckOverflow(); return temp; }

    auto ReturnOldAndAlloc() { 
        FullStream stream { _begin, _end, _curr };
        _begin = reinterpret_cast<uint8_t *>(malloc(limits->MinReadBuffer));
        if (_begin == nullptr) throw exception::MemoryAllocationException { }; 
        _curr = _begin;
        _end = _begin + limits->MinReadBuffer;
        return stream;
    }
};

class FixedBuffer {
    uint8_t *_begin;
    uint8_t *_end;

public:
    FixedBuffer(auto *_begin, auto *_end) : _begin { reinterpret_cast<uint8_t *>(_begin) }, _end { reinterpret_cast<uint8_t *>(_end) } { }
    FixedBuffer(auto *_begin, size_t size) : _begin { reinterpret_cast<uint8_t *>(_begin) }, _end { reinterpret_cast<uint8_t *>(_begin) + size } { }
    FixedBuffer(FixedBuffer &&buffer) : _begin { buffer._begin }, _end { buffer._end } { buffer._begin = buffer._end = nullptr;}
    FixedBuffer(FullStream &&stream) : _begin { stream._begin }, _end { stream._curr } { stream._begin = stream._curr = stream._end = nullptr;}
    FixedBuffer(const FixedBuffer &) = delete;
    ~FixedBuffer() { if (_begin) free(_begin); }

    FixedBuffer &operator=(const FixedBuffer &) = delete;

    auto begin() { return _begin; }
    const auto begin() const { return _begin; }

    // Curr is require for making this compatible with Stream
    auto curr() { return _begin; }
    const auto curr() const { return _begin; }

    auto end() { return _end; }
    const auto end() const { return _end; }

    auto Capacity() const { return static_cast<size_t>(_end - _begin); }

};


namespace typecheck {

template <typename T>
concept Stream = std::is_base_of_v<rohit::Stream, T>;

template <typename T>
concept WriteStream = std::is_base_of_v<rohit::Stream, T> || std::is_base_of_v<rohit::FixedBuffer, T>;

} // namespace typecheck


inline Stream MakeStream(auto *begin, auto *end) { return Stream { begin, end }; }
inline Stream MakeStream(auto *begin, size_t size) { return Stream { begin, size }; }
inline Stream MakeStream(std::string &string) { return Stream { string }; }

template <typename ChT> inline const Stream MakeConstantStream(const ChT *begin, const ChT *end) { return Stream { const_cast<ChT *>(begin), const_cast<ChT *>(end) }; }
template <typename ChT> inline const Stream MakeConstantStream(const ChT *begin, size_t size) { return Stream { const_cast<ChT *>(begin), size }; }
inline const Stream MakeConstantStream(const std::string &string) { return Stream { const_cast<char *>(string.data()), string.size() }; }

template <typename ChT> inline const FullStream MakeConstantFullStream(const ChT *begin, const ChT *end) { return FullStream { const_cast<ChT *>(begin), const_cast<ChT *>(end) }; }
template <typename ChT> inline const FullStream MakeConstantFullStream(const ChT *begin, size_t size) { return FullStream { const_cast<ChT *>(begin), size }; }
inline const FullStream MakeConstantFullStream(const std::string &string) { return FullStream { const_cast<char *>(string.data()), string.size() }; }
template <typename ChT> inline const FullStream MakeConstantFullStream(const ChT *begin, const ChT *end, const ChT *curr) { return FullStream { const_cast<ChT *>(begin), const_cast<ChT *>(end), const_cast<ChT *>(curr) }; }
inline const StreamAutoFree MakeStreamFromFile(const std::filesystem::path &path) {
    if (!std::filesystem::is_regular_file(path)) {
        throw std::invalid_argument { "Not a valid file" };
    }

    std::ifstream filestream { path, std::ios::binary | std::ios::ate };
    if (!filestream.is_open()) throw std::runtime_error { "Unable to open file" };

    filestream.seekg(0, std::ios::end);
    size_t size = filestream.tellg();
    filestream.seekg(0, std::ios::beg);

    auto buffer = reinterpret_cast<char *>(malloc(size));
    filestream.read(buffer, size);
    size = filestream.gcount();

    filestream.close();
    return StreamAutoFree { buffer, size };
}

namespace exception {
class BaseParser : public std::exception {
protected:
    const std::string whats_err;

    static const auto CreateWhatString(const Stream &stream, const std::string &errorstr) {
        std::string whats_err { "Error: " };

        const FullStream *fullstream = dynamic_cast<const FullStream *>(&stream);

        if (fullstream) {
            whats_err += " - Location: ";
            if (fullstream->CurrentOffset() >= 168 ){
                std::string_view second {reinterpret_cast<const char *>(fullstream->curr()) - 160, 160};
                for(auto &current_ch: second) {
                    if ((current_ch >= 32 /* &&  current_ch <= 127 */) || current_ch == '\n' || current_ch == '\r' || current_ch == '\t') {
                        whats_err.push_back(current_ch);
                    } else whats_err.push_back('#');
                }
            } else {
                std::string_view initial {reinterpret_cast<const char *>(fullstream->begin()), fullstream->CurrentOffset()};
                for(auto &current_ch: initial) {
                    if ((current_ch >= 32 /* &&  current_ch <= 127 */) || current_ch == '\n' || current_ch == '\r' || current_ch == '\t') {
                        whats_err.push_back(current_ch);
                    } else whats_err.push_back('#');
                }
            }
            whats_err += " <-- failed here with error ";
            whats_err += errorstr;
            whats_err += " --| ";
        } else {
            whats_err += "Failed with error: ";
            whats_err += errorstr;
            whats_err += " --- ";
        }

        std::string_view last {reinterpret_cast<const char *>(stream.curr()), std::min<size_t>(80, stream.RemainingBuffer())};
        for(auto &current_ch: last) {
            if ((current_ch >= 32 /* &&  current_ch <= 127 */) || current_ch == '\n' || current_ch == '\r' || current_ch == '\t') {
                whats_err.push_back(current_ch);
            } else whats_err.push_back('#');
        }
        if (stream.RemainingBuffer() > 80) {
            whats_err += " ... more ";
            whats_err += std::to_string(stream.RemainingBuffer() - 80UL);
            whats_err += " characters.";
        }

        return whats_err;
    }

public:
    BaseParser(const Stream &stream, const std::string &errorstr) : whats_err { CreateWhatString(stream, errorstr) } { }
    BaseParser(const Stream &stream) : whats_err { CreateWhatString(stream, {}) } { }

    const char *what() const noexcept override {
        return whats_err.c_str();
    }
};

} // namespace exception

} // namespace rohit