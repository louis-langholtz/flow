#ifndef char_template_joiner_hpp
#define char_template_joiner_hpp

namespace flow::detail {

template <typename...> struct char_template_joiner;

template <
    template<char...> class Tpl,
    char ...Args1>
struct char_template_joiner<Tpl<Args1...>>
{
    using type = Tpl<Args1...>;
};

template <
    template<char...> class Tpl,
    char ...Args1,
    char ...Args2>
struct char_template_joiner<Tpl<Args1...>, Tpl<Args2...>>
{
    using type = Tpl<Args1..., Args2...>;
};

template <template <char...> class Tpl,
          char ...Args1,
          char ...Args2,
          typename ...Tail>
struct char_template_joiner<Tpl<Args1...>, Tpl<Args2...>, Tail...>
{
     using type = typename char_template_joiner<Tpl<Args1..., Args2...>, Tail...>::type;
};

}

#endif /* char_template_joiner_hpp */
