#include <sqlite_orm/sqlite_orm.h>
#include <type_traits>  //  std::is_same
#include <catch2/catch_all.hpp>

using namespace sqlite_orm;
using internal::alias_column_t;
using internal::alias_holder;
using internal::column_alias;
using internal::column_pointer;

template<class ColAlias, class E>
void do_assert() {
    STATIC_REQUIRE(std::is_same<ColAlias, E>::value);
}

template<class E, class ColAlias>
void runTest(ColAlias /*colRef*/) {
    do_assert<ColAlias, E>();
}

TEST_CASE("aliases") {
    SECTION("column alias expressions") {
        runTest<alias_holder<column_alias<'a'>>>(get<colalias_a>());
#ifdef SQLITE_ORM_CLASSTYPE_TEMPLATE_ARG_SUPPORTED
        runTest<alias_holder<column_alias<'a'>>>(get<"a"_col>());
        runTest<column_alias<'a'>>("a"_col);
        constexpr auto z_alias = "z"_alias("1"_cte);
        runTest<alias_column_t<alias_z<cte_1>, column_pointer<cte_1, alias_holder<column_alias<'a'>>>>>(
            alias_column<z_alias>("a"_col));
        runTest<alias_column_t<alias_z<cte_1>, column_pointer<cte_1, alias_holder<column_alias<'a'>>>>>(
            z_alias->*"a"_col);
#endif
#ifdef SQLITE_ORM_WITH_CTE
        runTest<column_alias<'1'>>(1_colalias);
#endif
    }
}
