#pragma once

#include <type_traits>  //  std::enable_if, std::is_same
#ifdef SQLITE_ORM_WITH_CPP20_ALIASES
#include <concepts>
#endif

#include "functional/cxx_core_features.h"
#include "functional/cxx_type_traits_polyfill.h"

namespace sqlite_orm {
    // C++ generic traits used throughout the library
    namespace internal {
        template<class T, class... Types>
        using is_any_of = polyfill::disjunction<std::is_same<T, Types>...>;

        // enable_if for types
        template<template<typename...> class Op, class... Args>
        using match_if = std::enable_if_t<Op<Args...>::value>;

        // enable_if for types
        template<template<typename...> class Op, class... Args>
        using match_if_not = std::enable_if_t<polyfill::negation_v<Op<Args...>>>;

        // enable_if for types
        template<class T, template<typename...> class Primary>
        using match_specialization_of = std::enable_if_t<polyfill::is_specialization_of_v<T, Primary>>;

        // enable_if for functions
        template<template<typename...> class Op, class... Args>
        using satisfies = std::enable_if_t<Op<Args...>::value, bool>;

        // enable_if for functions
        template<template<typename...> class Op, class... Args>
        using satisfies_not = std::enable_if_t<polyfill::negation<Op<Args...>>::value, bool>;

        // enable_if for functions
        template<class T, template<typename...> class Primary>
        using satisfies_is_specialization_of = std::enable_if_t<polyfill::is_specialization_of_v<T, Primary>, bool>;
    }

    // type name template aliases for syntactic sugar
    namespace internal {
        template<typename T>
        using type_t = typename T::type;

        template<typename T>
        using value_type_t = typename T::value_type;

        template<typename T>
        using field_type_t = typename T::field_type;

        template<typename T>
        using constraints_type_t = typename T::constraints_type;

        template<typename T>
        using object_type_t = typename T::object_type;

        template<typename T>
        using elements_type_t = typename T::elements_type;

        template<typename T>
        using table_type_t = typename T::table_type;

        template<typename T>
        using target_type_t = typename T::target_type;

        template<typename T>
        using on_type_t = typename T::on_type;

#ifdef SQLITE_ORM_WITH_CTE
        template<typename T>
        using cte_label_type_t = typename T::cte_label_type;

        template<typename T>
        using cte_object_type_t = typename T::cte_object_type;

        template<typename T>
        using cte_mapper_type_t = typename T::cte_mapper_type;

        // T::alias_type or nonesuch
        template<class T>
        using alias_holder_type_or_none = polyfill::detected<type_t, T>;

        template<class T>
        using alias_holder_type_or_none_t = typename alias_holder_type_or_none<T>::type;
#endif

#ifdef SQLITE_ORM_WITH_CPP20_ALIASES
        template<auto T>
        using auto_type_t = typename decltype(T)::type;
#endif

        template<typename T>
        using expression_type_t = typename T::expression_type;

        template<class As>
        using alias_type_t = typename As::alias_type;
    }

#ifdef SQLITE_ORM_CPP20_CONCEPTS_SUPPORTED
    template<class T>
    concept orm_names_type = requires { typename T::type; };
#endif
}
