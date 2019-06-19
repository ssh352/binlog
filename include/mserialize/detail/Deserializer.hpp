#ifndef MSERIALIZE_DETAIL_DESERIALIZER_HPP
#define MSERIALIZE_DETAIL_DESERIALIZER_HPP

#include <mserialize/detail/sequence_traits.hpp>
#include <mserialize/detail/type_traits.hpp>

#include <cstdint>
#include <stdexcept>
#include <string> // to_string
#include <type_traits>
#include <utility> // move

namespace mserialize {

template <typename T, typename InputStream>
void deserialize(T& out, InputStream& istream);

namespace detail {

// Invalid deserializer

struct InvalidDeserializer
{
  template <typename T, typename InputStream>
  static void deserialize(const T& /* t */, InputStream& /* istream */)
  {
    static_assert(always_false<T>::value, "T is not deserializable");
  }
};

// Forward declarations

template <typename Arithmetic>
struct ArithmeticDeserializer;

template <typename Sequence>
struct SequenceDeserializer;

// Builtin deserializer - one specialization for each category

template <typename T, typename = void>
struct BuiltinDeserializer : InvalidDeserializer {};

template <typename T>
struct BuiltinDeserializer<T, enable_spec_if<std::is_arithmetic<T>>>
  : ArithmeticDeserializer<T> {};

template <typename T>
struct BuiltinDeserializer<T, enable_spec_if<
    is_deserializable_iterator<sequence_iterator_t<T>>
>> : SequenceDeserializer<T> {};

// Deserializer - entry point

template <typename T>
struct Deserializer
{
  using type = BuiltinDeserializer<T>;
};

// Arithmetic deserializer

template <typename Arithmetic>
struct ArithmeticDeserializer
{
  template <typename InputStream>
  static void deserialize(Arithmetic& t, InputStream& istream)
  {
    istream.read(reinterpret_cast<char*>(&t), sizeof(Arithmetic));
  }
};

// Sequence deserializer

template <typename Sequence>
struct SequenceDeserializer
{
  template <typename InputStream>
  static void deserialize(Sequence& s, InputStream& istream)
  {
    std::uint32_t size;
    mserialize::deserialize(size, istream);

    resize(s, size);
    deserialize_elems(is_proxy_sequence<Sequence>{}, s, istream);
  }

private:
  /**
   * Ensure size(s) == new_size by s.resize(new_size).
   * @throw std::runtime_error if there's no resize and
   *        the current size != new_size.
   */
  static void resize(Sequence& s, std::uint32_t new_size)
  {
    // the third argument is used to rank overloads
    // https://stackoverflow.com/questions/34419045/
    resize_impl(s, new_size, 0);
  }

  template <typename Sequence2>
  static auto resize_impl(Sequence2& s, std::uint32_t new_size, int /* preferred overload */)
    -> decltype(s.resize(new_size))
  {
    return s.resize(new_size);
  }

  template <typename Sequence2>
  static void resize_impl(Sequence2& s, std::uint32_t new_size, char /* fallback */)
  {
    const auto size = sequence_size(s);
    if (size != new_size)
    {
      throw std::runtime_error(
        "Serialized sequence size = " + std::to_string(new_size)
        + " != " + std::to_string(size) + " = target size, "
        "and target cannot be .resize()-ed"
      );
    }
  }

  template <typename InputStream>
  static void deserialize_elems(std::false_type /* no proxy */, Sequence& s, InputStream& istream)
  {
    for (auto&& elem : s)
    {
      mserialize::deserialize(elem, istream);
    }
  }

  template <typename InputStream>
  static void deserialize_elems(std::true_type /* proxy */, Sequence& s, InputStream& istream)
  {
    // For some sequences (e.g: std::vector<bool>)
    // S::reference != S::value_type &
    // as a proxy object is used.

    using value_type = typename Sequence::value_type;

    for (auto&& proxy : s)
    {
      value_type elem;
      mserialize::deserialize(elem, istream);
      proxy = std::move(elem);
    }
  }
};

} // namespace detail
} // namespace mserialize

#endif // MSERIALIZE_DETAIL_DESERIALIZER_HPP
