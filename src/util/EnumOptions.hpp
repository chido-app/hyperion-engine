#ifndef ENUM_OPTIONS_H
#define ENUM_OPTIONS_H

#include <core/lib/FixedArray.hpp>
#include <HashCode.hpp>
#include <math/MathUtil.hpp>
#include <Util.hpp>

#include <Types.hpp>

#include <array>
#include <initializer_list>

namespace hyperion {

template <typename EnumType, typename ValueType, SizeType Sz>
class EnumOptions
{
public:
    using EnumOption_t = EnumType;
    using Ordinal_t = UInt64;
    using EnumValueArray_t = FixedArray<ValueType, Sz>;
    using EnumValuePair_t = std::pair<EnumType, ValueType>;

    using Iterator = typename EnumValueArray_t::Iterator;
    using ConstIterator = typename EnumValueArray_t::ConstIterator;

    // convert from attachment (2^x) into ordinal (0-5) for use as an array index
    static constexpr UInt64 EnumToOrdinal(UInt64 option)
        { return MathUtil::FastLog2_Pow2(option); }

    // convert from ordinal (0-5) into power-of-two for use as bit flags
    static constexpr UInt64 OrdinalToEnum(UInt64 ordinal)
        { return 1ull << ordinal; }

    static_assert(Sz != 0, "EnumOptions cannot have size of zero");
    static_assert(
        OrdinalToEnum(Sz - 1) < MathUtil::MaxSafeValue<EnumType>(),
        "Size too large; enum conversion would cause overflow. "
        "Try changing the enum's underlying type to a larger sized data type?"
    );

    EnumOptions() : m_values{} {}
    EnumOptions(const EnumOptions &other) : m_values(other.m_values) {}

    EnumOptions &operator=(const EnumOptions &other)
    {
        m_values = other.m_values;

        return *this;
    }

    EnumOptions(EnumOptions &&other) : m_values(std::move(other.m_values)) {}

    EnumOptions &operator=(EnumOptions &&other) noexcept
    {
        m_values = std::move(other.m_values);

        return *this;
    }

    EnumOptions(const EnumValueArray_t &array) : m_values(array) {}

    EnumOptions(const std::vector<EnumValuePair_t> &pairs)
        : m_values { }
    {
        for (const auto &item : pairs) {
            Set(item.first, item.second);
        }
    }

    ~EnumOptions() = default;

    constexpr EnumValuePair_t KeyValueAt(SizeType index) const
        { return std::make_pair(EnumOption_t(OrdinalToEnum(index)), m_values[index]); }

    constexpr EnumType KeyAt(SizeType index) const
        { return EnumOption_t(OrdinalToEnum(index)); }

    constexpr ValueType &ValueAt(SizeType index)
        { return m_values[index]; }

    constexpr const ValueType &ValueAt(SizeType index) const
        { return m_values[index]; }

    constexpr ValueType &Get(EnumOption_t enum_key)
        { return m_values[EnumToOrdinal(enum_key)]; }

    constexpr const ValueType &Get(EnumOption_t enum_key) const
        { return m_values[EnumToOrdinal(enum_key)]; }

    constexpr ValueType &operator[](EnumOption_t enum_key)
        { return m_values[EnumToOrdinal(enum_key)]; }

    constexpr const ValueType &operator[](EnumOption_t enum_key) const
        { return m_values[EnumToOrdinal(enum_key)]; }

    EnumOptions &Set(EnumOption_t enum_key, ValueType &&value)
    {
        Ordinal_t ord = EnumToOrdinal(enum_key);
        AssertThrow(ord < m_values.Size());

        m_values[ord] = std::move(value);

        return *this;
    }

    EnumOptions &Set(EnumOption_t enum_key, const ValueType &value)
    {
        Ordinal_t ord = EnumToOrdinal(enum_key);
        AssertThrow(ord < m_values.Size());

        m_values[ord] = value;

        return *this;
    }

    EnumOptions &Unset(EnumOption_t enum_key)
    {
        Ordinal_t ord = EnumToOrdinal(enum_key);
        AssertThrow(ord < m_values.Size());

        m_values[ord] = { };

        return *this;
    }

    constexpr SizeType Size() const
        { return m_values.Size(); }

    ValueType *Data()
        { return m_values.Data(); }

    const ValueType *Data() const
        { return m_values.Data(); }

    void Clear()
    {
        for (auto &value : m_values) {
            value = ValueType { };
        }
    }

    HYP_DEF_STL_ITERATOR(m_values);

    HashCode GetHashCode() const
    {
        HashCode hc;

        for (const auto &it : m_values) {
            hc.Add(it.GetHashCode());
        }

        return hc;
    }

private:
    EnumValueArray_t m_values;
};

} // namespace hyperion

#endif