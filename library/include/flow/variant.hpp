#ifndef variant_h
#define variant_h

#include <ostream>
#include <variant>

namespace flow {

namespace detail {

template<class T>
struct streamer {
    const T& val; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

template<class T> streamer(T) -> streamer<T>;

template<class T>
std::ostream& operator<<(std::ostream& os, streamer<T> s) {
    os << s.val;
    return os;
}

}

/// @brief Variant type.
/// @note Use this alias instead of <code>std::variant</code> directly to support output
/// streaming within the same namespace as the flow object to be streamed.
using std::variant;

/// @brief Variant stream output support.
/// @note Code for this came from StackOverflow user "T.C." dated Nov 7, 2017.
/// @see https://stackoverflow.com/a/47169101/7410358.
template<class... Ts>
std::ostream& operator<<(std::ostream& os, detail::streamer<variant<Ts...>> sv) {
   std::visit([&os](const auto& v) { os << detail::streamer{v}; }, sv.val);
   return os;
}

}

#endif /* variant_h */
