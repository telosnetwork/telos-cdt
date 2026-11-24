/**
 *  @file
 *  @copyright defined in eosio.cdt/LICENSE.txt
 */

#pragma once

#include <cstring>   // memcpy, memmove, memset, strlen
#include <algorithm> // std::min, std::swap
#include <cstdint>   // uintptr_t
#include <memory>    // std::unique_ptr
#include <variant>   // std::variant
#include <vector>    // std::vector

#include "datastream.hpp" // eosio::datastream
#include "print.hpp"      // eosio::internal_use_do_not_use::prints_l
#include "varint.hpp"     // eosio::unsigned_int

namespace eosio {

   class string {
   public:
      static constexpr size_t npos = -1;

      template <size_t N>
      constexpr string(const char (&str)[N])
         : _size{N-1}
         , _capacity{_size}
         , _begin{str}
      {
      }

      constexpr string()
         : _size{0}
         , _capacity{0}
         , _begin{""}
      {
      }

      constexpr string(const char* str, const size_t n)
      {
         if (n != 0 && str == nullptr)
            eosio::check(false, "eosio::string::construct");
         clone(n, grown_capacity(n), str);
      }

      constexpr string(const size_t n, const char c)
      {
         fill(n, c);
      }

      constexpr string(const string& str, const size_t pos, const size_t n)
      {
         const auto count = str.range_length(pos, n, "eosio::string::substr");
         clone(count, grown_capacity(count), str.data() + pos);
      }

      constexpr string(const string& str)
         : _size{str._size}
         , _capacity{str._capacity}
      {
         if (str.is_literal())
            _begin = std::get<const char*>(str._begin);
         else
            clone(str._size, str._capacity, str.data());
      }

      constexpr string(string&& str)
         : _size{str._size}
         , _capacity{str._capacity}
      {
         if (str.is_literal())
            _begin = std::get<const char*>(str._begin);
         else {
            _begin = std::move(std::get<uptr>(str._begin));
            str._begin = "";
            str._size = str._capacity = 0;
         }
      }

      string& operator=(const string& str) {
         if (&str == this)
            return *this;

         if (str.is_literal())
            assign_borrowed(str.cbegin(), str._size);
         else
            clone(str._size, str._capacity, str.data());

         return *this;
      }

      string& operator=(string&& str) {
         if (&str == this)
            return *this;

         if (str.is_literal())
            assign_borrowed(str.cbegin(), str._size);
         else {
            _size = str._size;
            _capacity = str._capacity;
            _begin = std::move(std::get<uptr>(str._begin));
            str._begin = "";
            str._size = str._capacity = 0;
         }

         return *this;
      }

      string& operator=(const char* str) {
         eosio::check(str != nullptr, "eosio::string::assign");
         assign_borrowed(str, strlen(str));
         return *this;
      }

      char& operator[](const size_t n) {
         if (is_literal())
            clone(_size, _capacity, std::get<const char*>(_begin));
         return std::get<uptr>(_begin).get()[n];
      }

      const char operator[](const size_t n) const {
         return (is_literal()) ? std::get<const char*>(_begin)[n] : std::get<uptr>(_begin).get()[n];
      }

      char& at(const size_t n) {
         eosio::check(0 <= n && n < _size, "eosio::string::at");
         return operator[](n);
      }

      const char at(const size_t n) const {
         eosio::check(0 <= n && n < _size, "eosio::string::at const");
         return operator[](n);
      }

      char& front() {
         return at(0);
      }

      const char front() const {
         return at(0);
      }

      char& back() {
         return at(_size-1);
      }

      const char back() const {
         return at(_size-1);
      }

      char* data() {
         return begin();
      }

      const char* data() const {
         return cbegin();
      }

      const char* c_str() const {
         return cbegin();
      }

      char* begin() {
         if (is_literal())
            clone(_size, grown_capacity(_capacity), cbegin());

         return std::get<uptr>(_begin).get();
      }

      const char* cbegin() const {
         return (is_literal()) ? std::get<const char*>(_begin) : std::get<uptr>(_begin).get();
      }

      char* end() {
         return begin()+_size;
      }

      const char* cend() const {
         return cbegin()+_size;
      }

      bool empty() const {
         return !_size;
      }

      size_t size() const {
         return _size;
      }

      size_t length() const {
         return _size;
      }

      size_t capacity() const {
         return _capacity;
      }

      size_t max_size() const {
         return npos;
      }

      void reserve(const size_t n) {
         if (_capacity < n)
            clone(_size, n, cbegin());
      }

      void shrink_to_fit() {
         _capacity = _size;
      }

      void clear() {
         if (is_literal())
            clone(0, _capacity, cbegin());
         else {
            _size = 0;
            std::get<uptr>(_begin)[0] = '\0';
         }
      }

      void resize(const size_t n) {
         if (is_literal() || n > _capacity)
            clone(std::min(_size, n), n > _capacity ? grown_capacity(n) : _capacity, cbegin());

         auto* buffer = std::get<uptr>(_begin).get();
         if (n > _size)
            memset(buffer + _size, '\0', n - _size);
         buffer[n] = '\0';
         _size = n;
      }

      void swap(string& str) {
         std::swap(*this, str);
      }

      void push_back(const char c) {
         *this += c;
      }

      void pop_back() {
         if (_size == 0)
            return;
         resize(_size - 1);
      }

      string substr(size_t pos = 0, size_t len = npos) const {
         return string(*this, pos, len);
      }

      size_t copy(char* s, size_t len, size_t pos = 0) const {
         const auto count = range_length(pos, len, "eosio::string::copy");
         if (count != 0) {
            eosio::check(s != nullptr, "eosio::string::copy");
            const auto offset = buffer_offset(s);
            if (!is_literal() && offset <= _capacity)
               eosio::check(offset <= _size && count <= _size - offset, "eosio::string::copy");
            memmove(s, cbegin() + pos, count);
         }
         return count;
      }

      string& insert(const size_t pos, const char* str) {
         eosio::check(str != nullptr, "eosio::string::insert");
         return insert(pos, str, strlen(str));
      }

      string& insert(const size_t pos, const char* str, const size_t len) {
         eosio::check(str != nullptr && pos <= _size, "eosio::string::insert");
         if (len == 0)
            return *this;
         eosio::check(len <= npos - 1 - _size, "eosio::string::capacity");

         const auto offset = buffer_offset(str);
         const bool aliased = offset <= _capacity;
         if (aliased)
            eosio::check(offset <= _size && len <= _size + 1 - offset, "eosio::string::insert");

         const auto new_size = _size + len;
         if (new_size > _capacity || is_literal()) {
            const auto capacity = new_size > _capacity ? grown_capacity(new_size) : _capacity;
            auto buffer = allocate(capacity);
            const auto* old = cbegin();
            memcpy(buffer.get(), old, pos);
            memcpy(buffer.get() + pos, str, len);
            memcpy(buffer.get() + pos + len, old + pos, _size - pos);
            buffer[new_size] = '\0';
            _begin = std::move(buffer);
            _capacity = capacity;
         } else {
            // Moving the suffix can overwrite an insertion source in the same buffer.
            uptr source;
            if (aliased) {
               source = allocate(len);
               memcpy(source.get(), str, len);
               str = source.get();
            }
            auto* buffer = std::get<uptr>(_begin).get();
            memmove(buffer + pos + len, buffer + pos, _size - pos);
            memcpy(buffer + pos, str, len);
            buffer[new_size] = '\0';
         }
         _size = new_size;

         return *this;
      }

      string& insert(const size_t pos, const string& str) {
         return insert(pos, str.c_str());
      }

      string& erase(size_t pos = 0, size_t len = npos) {
         const auto count = range_length(pos, len, "eosio::string::erase");
         if (count != 0) {
            if (is_literal())
               clone(_size, _capacity, cbegin());
            auto* buffer = std::get<uptr>(_begin).get();
            memmove(buffer + pos, buffer + pos + count, _size - pos - count);
            _size -= count;
            buffer[_size] = '\0';
         }

         return *this;
      }

      string& append(const char* str) {
         eosio::check(str != nullptr, "eosio::string::append");
         insert(_size, str);

         return *this;
      }

      string& append(const string& str) {
         insert(_size, str);
         return *this;
      }

      string& operator+=(const char c) {
         return insert(_size, &c, 1);
      }

      string& operator+=(const char* rhs) {
         append(rhs);
         return *this;
      }

      string& operator+=(const string& rhs) {
         append(rhs);
         return *this;
      }

      inline void print() const {
         const char* tmp{(is_literal()) ? std::get<const char*>(_begin) : std::get<uptr>(_begin).get()};
         internal_use_do_not_use::prints_l(tmp, _size);
      }

      friend bool operator< (const string& lhs, const string& rhs);
      friend bool operator> (const string& lhs, const string& rhs);
      friend bool operator<=(const string& lhs, const string& rhs);
      friend bool operator>=(const string& lhs, const string& rhs);
      friend bool operator==(const string& lhs, const string& rhs);
      friend bool operator!=(const string& lhs, const string& rhs);

      friend string operator+ (const string& lhs, const string& rhs);

   private:
      using uptr    = std::unique_ptr<char[]>;
      using sso_str = std::variant<const char*, uptr>;

      size_t  _size     = 0;
      size_t  _capacity = 0;
      sso_str _begin    = nullptr;

      constexpr size_t range_length(size_t pos, size_t count, const char* message) const {
         if (pos > _size)
            eosio::check(false, message);
         return std::min(count, _size - pos);
      }

      static constexpr size_t grown_capacity(size_t size) {
         return size <= (npos - 1) / 2 ? size * 2 : size;
      }

      static uptr allocate(size_t capacity) {
         eosio::check(capacity < npos, "eosio::string::capacity");
         return uptr{new char[capacity + 1]};
      }

      uintptr_t buffer_offset(const char* str) const {
         // A source can point outside the string, where pointer subtraction is invalid.
         return reinterpret_cast<uintptr_t>(str) - reinterpret_cast<uintptr_t>(cbegin());
      }

      void assign_borrowed(const char* str, size_t size) {
         if (!is_literal() && buffer_offset(str) <= _capacity) {
            clone(size, size, str);
         } else {
            _size = _capacity = size;
            _begin = str;
         }
      }

      void clone(size_t size, size_t capacity, const char* str) {
         eosio::check(size <= capacity, "eosio::string::capacity");
         auto begin = allocate(capacity);
         if (size != 0)
            memcpy(begin.get(), str, size);
         begin[size] = '\0';
         _begin = std::move(begin);
         _size     = size;
         _capacity = capacity;
      }

      void fill(size_t size, char value) {
         const auto capacity = grown_capacity(size);
         auto begin = allocate(capacity);
         memset(begin.get(), value, size);
         begin[size] = '\0';
         _begin = std::move(begin);
         _size = size;
         _capacity = capacity;
      }

      constexpr bool is_literal() const {
         return (std::holds_alternative<const char*>(_begin)) ? true : false;
      }
   };

   inline bool operator< (const string& lhs, const string& rhs) {
      const char* beg_lhs{lhs.cbegin()}; const char* end_lhs{lhs.cend()};
      const char* beg_rhs{rhs.cbegin()}; const char* end_rhs{rhs.cend()};

      for(; beg_lhs != end_lhs && beg_rhs != end_rhs; ++beg_lhs, ++beg_rhs) {
         if (*beg_lhs < *beg_rhs)
            return true;
         if (*beg_rhs < *beg_lhs)
            return false;
      }

      return beg_lhs == end_lhs && beg_rhs != end_rhs;
   }

   inline bool operator> (const string& lhs, const string& rhs) {
      return (rhs < lhs);
   }

   inline bool operator<=(const string& lhs, const string& rhs) {
      return !(rhs < lhs);
   }

   inline bool operator>=(const string& lhs, const string& rhs) {
      return !(lhs < rhs);
   }

   inline bool operator==(const string& lhs, const string& rhs) {
      return !(lhs < rhs) && !(rhs < lhs);
   }

   inline bool operator!=(const string& lhs, const string& rhs) {
      return !(lhs == rhs);
   }

   inline string operator+(const string& lhs, const string& rhs) {
      string res{lhs};
      res += rhs;
      return res;
   }

   template<typename DataStream>
   DataStream& operator<<(DataStream& ds, const string& str) {
      ds << unsigned_int(str.size());
      if (str.size())
         ds.write(str.data(), str.size());
      return ds;
   }

   template<typename DataStream>
   DataStream& operator>>(DataStream& ds, string& str) {
      std::vector<char> tmp;
      ds >> tmp;
      str = (tmp.size()) ? string(tmp.data(), tmp.size()) : string();
      return ds;
   }

} // namespace eosio
