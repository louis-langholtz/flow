#ifndef reserved_hpp
#define reserved_hpp

namespace flow::reserved {

constexpr auto unset_endpoint_prefix = '-';
constexpr auto user_endpoint_prefix = '^';
constexpr auto file_endpoint_prefix = '%';
constexpr auto address_prefix = '@';
constexpr auto descriptors_prefix = ':';

constexpr auto node_name_separator = '.';
constexpr auto descriptor_separator = ',';
constexpr auto env_separator = '=';

}

#endif /* reserved_hpp */
