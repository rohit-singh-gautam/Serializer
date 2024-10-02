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

namespace rohit {

namespace exception {
class StreamOverflowException : std::exception {
public:
    const char *what() const noexcept override {
        return "Stream Overflow";
    }
}; // class StreamOverflowException

class StreamUnderflowException : std::exception {
public:
    const char *what() const noexcept override {
        return "Stream Underflow";
    }
}; // class StreamUnderflowException

class MemoryAllocationException : std::exception {
public:
    const char *what() const noexcept override {
        return "Stream unable to allocate memory";
    }
}; // class MemoryAllocationException

} // namespace exception

class Stream {
protected:
    friend class FixedBuffer;
    mutable uint8_t *_curr;
    uint8_t * _end;

    void CheckOverflow() const { if (_curr >= _end) throw exception::StreamOverflowException { }; }
    void CheckOverflow(size_t len) const { if (_curr + len > _end) throw exception::StreamOverflowException { }; }

    Stream() : _curr { nullptr }, _end { nullptr } { }
public:
    constexpr Stream(auto *_begin, auto *_end) : _curr { reinterpret_cast<uint8_t *>(_begin) }, _end { reinterpret_cast<uint8_t *>(_end) } { }
    constexpr Stream(auto *_begin, size_t size) : _curr { reinterpret_cast<uint8_t *>(_begin) }, _end { _curr + size } { }
    constexpr Stream(Stream &&stream) : _curr { stream._curr }, _end { stream._end } { stream._curr = stream._end = nullptr; }
    constexpr Stream(std::string &string) : _curr { reinterpret_cast<uint8_t *>(string.data()) }, _end { _curr + string.size() } { }
    Stream(const Stream &stream) : _curr { stream._curr }, _end { stream._end } { }
    virtual ~Stream() = default;

    constexpr Stream GetSimpleStream() { return Stream { _curr, _end }; }
    constexpr const Stream GetSimpleStream() const { return Stream { _curr, _end }; }
    constexpr const Stream GetSimpleConstStream() const { return Stream { _curr, _end }; }

    template <size_t _size>
    bool operator==(const auto (&data)[_size + 1]) const {
        if (remaining_buffer() < _size) return false;
        return std::equal(std::begin(data), std::end(data), _curr);
    }

    /*! IMPORTANT: text must not be null terminated */
    bool operator==(const std::string_view &text) const {
        if (remaining_buffer() < text.size()) return false;
        return std::equal(std::begin(text), std::end(text), _curr);
    }

    Stream &operator=(const Stream &stream) { _curr = stream._curr; return *this; }
    const Stream &operator=(const Stream &stream) const { _curr = stream._curr; return *this; }
    virtual Stream operator+(size_t len) { _curr += len; return *this; }
    virtual const Stream operator+(size_t len) const { _curr += len; return *this; }
    virtual Stream &operator+=(size_t len) { _curr += len; return *this; }
    virtual const Stream &operator+=(size_t len) const { _curr += len; return *this; }

    constexpr inline uint8_t &operator*() { return *_curr; }
    constexpr inline uint8_t operator*() const { return *_curr; }
    constexpr inline uint8_t &at_unchecked() { return *_curr; }
    constexpr inline uint8_t at_unchecked() const { return *_curr; }
    virtual constexpr inline Stream &operator++() { ++_curr; return *this; }
    virtual constexpr inline Stream &operator--() { --_curr; return *this;}
    virtual constexpr inline const Stream &operator++() const { ++_curr; return *this; }
    virtual constexpr inline const Stream &operator--() const { --_curr; return *this;}

    virtual constexpr inline uint8_t *GetCurrAndIncrease(const size_t len) { auto temp = _curr; _curr += len; return temp; }
    virtual constexpr inline const uint8_t *GetCurrAndIncrease(const size_t len) const { auto temp = _curr; _curr += len; return temp; }

    size_t GetSizeFrom(const auto *start) const { return static_cast<size_t>(_curr - reinterpret_cast<const uint8_t *>(start)); }

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Weffc++"
    virtual constexpr inline uint8_t *operator++(int) { uint8_t *temp = _curr; ++_curr; return temp; };
    virtual constexpr inline uint8_t *operator--(int) { uint8_t *temp = _curr; --_curr; return temp; };
    virtual constexpr inline const uint8_t *operator++(int) const { uint8_t *temp = _curr; ++_curr; return temp; };
    virtual constexpr inline const uint8_t *operator--(int) const { uint8_t *temp = _curr; --_curr; return temp; };
    #pragma GCC diagnostic pop

    const uint8_t *end() const { return _end; }
    const uint8_t *curr() const { return _curr; }

    auto &end() { return _end; }
    auto &curr() { return _curr; }

    size_t remaining_buffer() const { return static_cast<size_t>(_end - _curr); }

    bool full() const { return _curr == _end; }

    template <typename T>
    bool CheckCapacity() const { return remaining_buffer() >= sizeof(T); }
    bool CheckCapacity(const size_t size) const { return remaining_buffer() >= size; }

    void UpdateCurr(uint8_t *_curr) const { this->_curr = _curr; }

    auto GetRawCurrentBuffer() { return std::make_pair(_curr, remaining_buffer()); }

    virtual constexpr inline void Reserve(const size_t len) { CheckOverflow(len); }
    constexpr inline void Reserve(const auto *_begin, const auto *_end) { 
        const size_t len = reinterpret_cast<const char *>(_end) - reinterpret_cast<const char *>(_begin);
        CheckOverflow(len);
    }
    constexpr inline void Copy(const std::string &source) { Reserve(source.size()); _curr = std::copy(std::begin(source), std::end(source), _curr); }
    constexpr inline void Copy(const Stream &source) { Reserve(source.remaining_buffer()); _curr = std::copy(source.curr(), source.end(), _curr); }
    constexpr inline void Copy(const auto *begin, const auto *end) { Reserve(begin, end); _curr = std::copy(reinterpret_cast<const uint8_t *>(begin), reinterpret_cast<const uint8_t *>(end), _curr); }
    constexpr inline void Copy(const auto *begin, size_t size) { Reserve(size); _curr = std::copy(reinterpret_cast<const uint8_t *>(begin), reinterpret_cast<const uint8_t *>(begin) + size, _curr); }

    template <typename ValueType>
    constexpr inline void Copy(const ValueType &value) {
        if constexpr (std::is_same_v<ValueType, char>) {
            at_unchecked() = value;
            operator++();
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
        } else static_assert(false, "Unsupported type");
    }

    template<typename... ValueType> 
    constexpr inline void Write(const ValueType& ...value) {
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
    constexpr FullStream(auto *_begin, auto *_end) : Stream {reinterpret_cast<uint8_t *>(_begin), reinterpret_cast<uint8_t *>(_end)}, _begin { reinterpret_cast<uint8_t *>(_begin) } { }
    constexpr FullStream(auto *_begin, auto *_end, auto *_curr) : Stream {reinterpret_cast<uint8_t *>(_curr), reinterpret_cast<uint8_t *>(_end)}, _begin { reinterpret_cast<uint8_t *>(_begin) } { }
    constexpr FullStream(auto *_begin, size_t size) :  Stream {reinterpret_cast<uint8_t *>(_begin), size}, _begin { reinterpret_cast<uint8_t *>(_begin) } { }
    constexpr FullStream(FullStream &&stream) : Stream { std::move(stream) }, _begin { stream._begin } { stream._begin = nullptr; }
    FullStream(const FullStream &stream) : Stream { stream }, _begin { stream._begin } { }

    FullStream &operator=(const FullStream &stream) { _curr = stream._curr; return *this; }

    const auto begin() const { return _begin; }
    auto begin() { return _begin; }

    auto index() const { return static_cast<size_t>(_curr - _begin); }
    auto capacity() const { return static_cast<size_t>(_end - _begin); }

    uint8_t *Move() {
        auto temp = _begin;
        _begin = _end = _curr = nullptr;
        return temp;
    }

    bool IsNull() const { return _begin == nullptr; }
    auto GetRawFullBuffer() { return std::make_pair(_begin, capacity()); }
    void Reset() const { _curr = _begin; }
    bool empty() const { return _curr == _begin; }
};

class StreamAutoFree : public FullStream {
public:
    ~StreamAutoFree() { free(_begin); }
};

class FullStreamLimitChecked : public FullStream {
public:
    using FullStream::FullStream;
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Weffc++"
    constexpr inline Stream &operator--() override { CheckUnderflow(); --_curr; return *this;}
    constexpr inline const Stream &operator--() const override { CheckUnderflow(); --_curr; return *this;}
    constexpr inline const uint8_t *operator--(int) const override { CheckUnderflow(); const uint8_t *temp = _curr; --_curr; return temp; };

    Stream operator+(size_t len) override { _curr += len; CheckOverflow(); return *this; }
    const Stream operator+(size_t len) const override { _curr += len; CheckOverflow(); return *this; }
    Stream &operator+=(size_t len) override { _curr += len; CheckOverflow(); return *this; }
    const Stream &operator+=(size_t len) const override { _curr += len; CheckOverflow(); return *this; }
    constexpr inline Stream &operator++() override { ++_curr; CheckOverflow(); return *this; }
    constexpr inline const Stream &operator++() const override { ++_curr; CheckOverflow(); return *this; }
    constexpr inline const uint8_t *GetCurrAndIncrease(const size_t len) const override { auto temp = _curr; CheckOverflow(); _curr += len; return temp; }
    #pragma GCC diagnostic pop
};

class FullStreamAutoAlloc : public FullStream {
    constexpr inline void CheckResize() {
        if (_curr == _end) {
            auto curr_index = index();
            auto new_capacity = capacity() * 2;
            _begin = reinterpret_cast<uint8_t *>(realloc(reinterpret_cast<void *>(_begin), new_capacity));
            _end = _begin + new_capacity;
            _curr = _begin + curr_index;
        }
    }

    constexpr inline void CheckResize(const size_t len) {
        if (_curr + len > _end) {
            auto curr_index = index();
            auto new_capacity = capacity() * 2;
            while(curr_index + len > new_capacity) new_capacity += capacity();
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

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Weffc++"
    constexpr inline Stream &operator--() override { CheckUnderflow(); --_curr; return *this;}
    constexpr inline const Stream &operator--() const override { CheckUnderflow(); --_curr; return *this;}
    constexpr inline Stream &operator++() override { CheckResize(); ++_curr; return *this; }
    constexpr inline const Stream &operator++() const override { CheckOverflow(); ++_curr; return *this; }
    constexpr inline uint8_t *operator++(int) override { CheckResize(); uint8_t *temp = _curr; ++_curr; return temp; };
    constexpr inline const uint8_t *operator++(int) const override { CheckOverflow(); uint8_t *temp = _curr; ++_curr; return temp; };
    constexpr inline Stream operator+(size_t len) override { CheckResize(len); _curr += len; return *this; }
    constexpr inline  const Stream operator+(size_t len) const override { _curr += len; CheckOverflow(); return *this; }
    constexpr inline Stream &operator+=(size_t len) override { CheckResize(len); _curr += len; return *this; }
    constexpr inline const Stream &operator+=(size_t len) const override { _curr += len; CheckOverflow(); return *this; }
    #pragma GCC diagnostic pop


    constexpr inline void Reserve(const size_t len) override { CheckResize(len); }

    constexpr inline uint8_t *GetCurrAndIncrease(const size_t len) override { CheckResize(len); auto temp = _curr; _curr += len; return temp; }
    constexpr inline const uint8_t *GetCurrAndIncrease(const size_t len) const override { auto temp = _curr; _curr += len; CheckOverflow(); return temp; }

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

    constexpr inline void CheckResize() {
        if (_curr == _end) {
            auto curr_index = index();
            auto new_capacity = capacity() * 2;
            if (new_capacity > limits->MaxReadBuffer) throw exception::StreamOverflowException { };
            _begin = reinterpret_cast<uint8_t *>(realloc(reinterpret_cast<void *>(_begin), new_capacity));
            if (_begin == nullptr) throw exception::MemoryAllocationException { };
            _end = _begin + new_capacity;
            _curr = _begin + curr_index;
        }
    }

    constexpr inline void CheckResize(const size_t len) {
        if (_curr + len > _end) {
            auto curr_index = index();
            auto new_capacity = capacity() * 2;
            while(curr_index + len > new_capacity) new_capacity += capacity();
            if (new_capacity > limits->MaxReadBuffer) throw exception::StreamOverflowException { };
            _begin = reinterpret_cast<uint8_t *>(realloc(reinterpret_cast<void *>(_begin), new_capacity));
            if (_begin == nullptr) throw exception::MemoryAllocationException { };
            _end = _begin + new_capacity;
            _curr = _begin + curr_index;
        }
    }

    constexpr inline void SetMinBuffer() {
        auto current_buffer_size = static_cast<size_t>(_end - _begin);
        if (current_buffer_size < limits->MinReadBuffer) {
            auto curr_index = index();
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
    FullStreamAutoAllocLimits(const FullStreamAutoAllocLimits &) = delete;
    FullStreamAutoAllocLimits &operator=(const FullStreamAutoAllocLimits &) = delete;

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Weffc++"
    constexpr inline Stream &operator--() override { CheckUnderflow(); --_curr; return *this;}
    constexpr inline const Stream &operator--() const override { CheckUnderflow(); --_curr; return *this;}
    constexpr inline Stream &operator++() override { CheckResize(); ++_curr; return *this; }
    constexpr inline const Stream &operator++() const override { CheckOverflow(); ++_curr; return *this; }
    constexpr inline uint8_t *operator++(int) override { CheckResize(); uint8_t *temp = _curr; ++_curr; return temp; };
    constexpr inline const uint8_t *operator++(int) const override { CheckOverflow(); uint8_t *temp = _curr; ++_curr; return temp; };
    constexpr inline Stream operator+(size_t len) override { CheckResize(len); _curr += len; return *this; }
    constexpr inline  const Stream operator+(size_t len) const override { _curr += len; CheckOverflow(); return *this; }
    constexpr inline Stream &operator+=(size_t len) override { CheckResize(len); _curr += len; return *this; }
    constexpr inline const Stream &operator+=(size_t len) const override { _curr += len; CheckOverflow(); return *this; }
    #pragma GCC diagnostic pop


    constexpr inline void Reserve(const size_t len) override { CheckResize(len); }

    constexpr inline uint8_t *GetCurrAndIncrease(const size_t len) override { CheckResize(len); auto temp = _curr; _curr += len; return temp; }
    constexpr inline const uint8_t *GetCurrAndIncrease(const size_t len) const override { auto temp = _curr; _curr += len; CheckOverflow(); return temp; }

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

    auto size() const { return static_cast<size_t>(_end - _begin); }

};


namespace typecheck {

template <typename T>
concept Stream = std::is_base_of_v<rohit::Stream, T>;

template <typename T>
concept WriteStream = std::is_base_of_v<rohit::Stream, T> || std::is_base_of_v<rohit::FixedBuffer, T>;

} // namespace typecheck


constexpr inline Stream make_stream(auto *begin, auto *end) { return Stream { begin, end }; }
constexpr inline Stream make_stream(auto *begin, size_t size) { return Stream { begin, size }; }
constexpr inline Stream make_stream(std::string &string) { return Stream { string }; }

template <typename ChT> constexpr inline const Stream make_const_stream(const ChT *begin, const ChT *end) { return Stream { const_cast<ChT *>(begin), const_cast<ChT *>(end) }; }
template <typename ChT> constexpr inline const Stream make_const_stream(const ChT *begin, size_t size) { return Stream { const_cast<ChT *>(begin), size }; }
constexpr inline const Stream make_const_stream(const std::string &string) { return Stream { const_cast<char *>(string.data()), string.size() }; }

template <typename ChT> constexpr inline const FullStream make_const_fullstream(const ChT *begin, const ChT *end) { return FullStream { const_cast<ChT *>(begin), const_cast<ChT *>(end) }; }
template <typename ChT> constexpr inline const FullStream make_const_fullstream(const ChT *begin, size_t size) { return FullStream { const_cast<ChT *>(begin), size }; }
constexpr inline const FullStream make_const_fullstream(const std::string &string) { return FullStream { const_cast<char *>(string.data()), string.size() }; }
template <typename ChT> constexpr inline const FullStream make_const_fullstream(const ChT *begin, const ChT *end, const ChT *curr) { return FullStream { const_cast<ChT *>(begin), const_cast<ChT *>(end), const_cast<ChT *>(curr) }; }

} // namespace rohit