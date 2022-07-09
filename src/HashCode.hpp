#ifndef HASH_CODE_H
#define HASH_CODE_H

#include <Types.hpp>
#include <Constants.hpp>

#include <type_traits>
#include <functional>
#include <string>

namespace hyperion {
struct HashCode {
    using ValueType = UInt64;

    HashCode()
        : hash(0)
    {
    }

    HashCode(const HashCode &other)
        : hash(other.hash)
    {
    }

    constexpr bool operator==(const HashCode &other) const { return hash == other.hash; }
    constexpr bool operator!=(const HashCode &other) const { return hash != other.hash; }

    template<class T>
    typename std::enable_if_t<std::is_same_v<T, HashCode> || std::is_base_of_v<HashCode, T>>
    Add(const T &hash_code) { HashCombine(hash_code.Value()); }

    template<class T>
    typename std::enable_if_t<!(std::is_same_v<T, HashCode> || std::is_base_of_v<HashCode, T>) && (std::is_fundamental_v<T> || std::is_pointer_v<T> || implementation_exists<std::hash<T>>)>
    Add(const T &value)     { HashCombine(std::hash<T>()(value)); }

    template<class T>
    typename std::enable_if_t<!(std::is_same_v<T, HashCode> || std::is_base_of_v<HashCode, T>) && std::is_class_v<std::decay_t<T>> && !std::is_pod_v<T> && !implementation_exists<std::hash<T>>>
    Add(const T &value)     { HashCombine(std::hash<T>()(value.GetHashCode())); }

    constexpr ValueType Value() const { return hash; }

private:
    ValueType hash;

    constexpr void HashCombine(ValueType other)
    {
        hash ^= other + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
};
} // namespace hyperion

#endif
