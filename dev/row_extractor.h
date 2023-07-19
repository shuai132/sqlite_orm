#pragma once

#include <sqlite3.h>
#include <type_traits>  //  std::enable_if_t, std::is_arithmetic, std::is_same, std::enable_if
#include <stdlib.h>  //  atof, atoi, atoll
#include <system_error>  //  std::system_error
#include <string>  //  std::string, std::wstring
#ifndef SQLITE_ORM_OMITS_CODECVT
#include <codecvt>  //  std::wstring_convert, std::codecvt_utf8_utf16
#endif  //  SQLITE_ORM_OMITS_CODECVT
#include <vector>  //  std::vector
#include <cstring>  //  strlen
#include <locale>
#include <algorithm>  //  std::copy
#include <iterator>  //  std::back_inserter
#include <tuple>  //  std::tuple, std::tuple_size, std::tuple_element
#ifdef SQLITE_ORM_CPP20_CONCEPTS_SUPPORTED
#include <concepts>
#endif

#include "functional/cxx_universal.h"
#include "arithmetic_tag.h"
#include "pointer_value.h"
#include "journal_mode.h"
#include "error_code.h"
#include "is_std_ptr.h"

namespace sqlite_orm {

    /**
     *  Helper for casting values originating from SQL to C++ typed values, usually from rows of a result set.
     *  
     *  sqlite_orm provides specializations for known C++ types, users may define their custom specialization
     *  of this helper.
     *  
     *  @note (internal): Since row extractors are used in certain contexts with only one purpose at a time
     *                    (e.g., converting a row result set but not function values or column text),
     *                    there are factory functions that perform conceptual checking that should be used
     *                    instead of directly creating row extractors.
     *  
     *  
     */
    template<class V, typename Enable = void>
    struct row_extractor {
        /*
         *  Called during one-step query execution (one result row) for each column of a result row.
         */
        V extract(const char* columnText) const = delete;

        /*
         *  Called during multi-step query execution (result set) for each column of a result row.
         */
        V extract(sqlite3_stmt* stmt, int columnIndex) const = delete;

        /*
         *  Called before invocation of user-defined scalar or aggregate functions,
         *  in order to unbox dynamically typed SQL function values into a tuple of C++ function arguments.
         */
        V extract(sqlite3_value* value) const = delete;
    };

#ifdef SQLITE_ORM_CPP20_CONCEPTS_SUPPORTED
    template<typename T>
    concept orm_column_text_extractable = requires(const row_extractor<T>& extractor, const char* columnText) {
                                              { extractor.extract(columnText) } -> std::same_as<T>;
                                          };

    template<typename T>
    concept orm_row_value_extractable =
        requires(const row_extractor<T>& extractor, sqlite3_stmt* stmt, int columnIndex) {
            { extractor.extract(stmt, columnIndex) } -> std::same_as<T>;
        };

    template<typename T>
    concept orm_boxed_value_extractable = requires(const row_extractor<T>& extractor, sqlite3_value* value) {
                                              { extractor.extract(value) } -> std::same_as<T>;
                                          };
#endif

    namespace internal {
        /*  
         *  Make a row extractor to be used for casting SQL column text to a C++ typed value.
         */
        template<class R>
#ifdef SQLITE_ORM_CPP20_CONCEPTS_SUPPORTED
            requires(orm_column_text_extractable<R>)
#endif
        row_extractor<R> column_text_extractor() {
            return {};
        }

        /*  
         *  Make a row extractor to be used for converting a value from a SQL result row set to a C++ typed value.
         */
        template<class R>
#ifdef SQLITE_ORM_CPP20_CONCEPTS_SUPPORTED
            requires(orm_row_value_extractable<R>)
#endif
        row_extractor<R> row_value_extractor() {
            return {};
        }

        /*  
         *  Make a row extractor to be used for unboxing a dynamically typed SQL value to a C++ typed value.
         */
        template<class R>
#ifdef SQLITE_ORM_CPP20_CONCEPTS_SUPPORTED
            requires(orm_boxed_value_extractable<R>)
#endif
        row_extractor<R> boxed_value_extractor() {
            return {};
        }
    }

    template<class R>
    int extract_single_value(void* data, int argc, char** argv, char**) {
        auto& res = *(R*)data;
        if(argc) {
            const auto rowExtractor = internal::column_text_extractor<R>();
            res = rowExtractor.extract(argv[0]);
        }
        return 0;
    }

    /**
     *  Specialization for the 'pointer-passing interface'.
     * 
     *  @note The 'pointer-passing' interface doesn't support (and in fact prohibits)
     *  extracting pointers from columns.
     */
    template<class P, class T>
    struct row_extractor<pointer_arg<P, T>, void> {
        using V = pointer_arg<P, T>;

        V extract(const char* columnText) const = delete;

        V extract(sqlite3_stmt* stmt, int columnIndex) const = delete;

        V extract(sqlite3_value* value) const {
            return {(P*)sqlite3_value_pointer(value, T::value)};
        }
    };

    /**
     * Undefine using pointer_binding<> for querying values
     */
    template<class P, class T, class D>
    struct row_extractor<pointer_binding<P, T, D>, void>;

    /**
     *  Specialization for arithmetic types.
     */
    template<class V>
    struct row_extractor<V, std::enable_if_t<std::is_arithmetic<V>::value>> {
        V extract(const char* columnText) const {
            return this->extract(columnText, tag());
        }

        V extract(sqlite3_stmt* stmt, int columnIndex) const {
            return this->extract(stmt, columnIndex, tag());
        }

        V extract(sqlite3_value* value) const {
            return this->extract(value, tag());
        }

      private:
        using tag = arithmetic_tag_t<V>;

        V extract(const char* columnText, const int_or_smaller_tag&) const {
            return static_cast<V>(atoi(columnText));
        }

        V extract(sqlite3_stmt* stmt, int columnIndex, const int_or_smaller_tag&) const {
            return static_cast<V>(sqlite3_column_int(stmt, columnIndex));
        }

        V extract(sqlite3_value* value, const int_or_smaller_tag&) const {
            return static_cast<V>(sqlite3_value_int(value));
        }

        V extract(const char* columnText, const bigint_tag&) const {
            return static_cast<V>(atoll(columnText));
        }

        V extract(sqlite3_stmt* stmt, int columnIndex, const bigint_tag&) const {
            return static_cast<V>(sqlite3_column_int64(stmt, columnIndex));
        }

        V extract(sqlite3_value* value, const bigint_tag&) const {
            return static_cast<V>(sqlite3_value_int64(value));
        }

        V extract(const char* columnText, const real_tag&) const {
            return static_cast<V>(atof(columnText));
        }

        V extract(sqlite3_stmt* stmt, int columnIndex, const real_tag&) const {
            return static_cast<V>(sqlite3_column_double(stmt, columnIndex));
        }

        V extract(sqlite3_value* value, const real_tag&) const {
            return static_cast<V>(sqlite3_value_double(value));
        }
    };

    /**
     *  Specialization for std::string.
     */
    template<class T>
    struct row_extractor<T, std::enable_if_t<std::is_base_of<std::string, T>::value>> {
        T extract(const char* columnText) const {
            if(columnText) {
                return columnText;
            } else {
                return {};
            }
        }

        T extract(sqlite3_stmt* stmt, int columnIndex) const {
            if(auto cStr = (const char*)sqlite3_column_text(stmt, columnIndex)) {
                return cStr;
            } else {
                return {};
            }
        }

        T extract(sqlite3_value* value) const {
            if(auto cStr = (const char*)sqlite3_value_text(value)) {
                return cStr;
            } else {
                return {};
            }
        }
    };
#ifndef SQLITE_ORM_OMITS_CODECVT
    /**
     *  Specialization for std::wstring.
     */
    template<>
    struct row_extractor<std::wstring, void> {
        std::wstring extract(const char* columnText) const {
            if(columnText) {
                std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
                return converter.from_bytes(columnText);
            } else {
                return {};
            }
        }

        std::wstring extract(sqlite3_stmt* stmt, int columnIndex) const {
            auto cStr = (const char*)sqlite3_column_text(stmt, columnIndex);
            if(cStr) {
                std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
                return converter.from_bytes(cStr);
            } else {
                return {};
            }
        }

        std::wstring extract(sqlite3_value* value) const {
            if(auto cStr = (const wchar_t*)sqlite3_value_text16(value)) {
                return cStr;
            } else {
                return {};
            }
        }
    };
#endif  //  SQLITE_ORM_OMITS_CODECVT

    template<class V>
    struct row_extractor<V, std::enable_if_t<is_std_ptr<V>::value>> {
        using unqualified_type = std::remove_cv_t<typename V::element_type>;

        V extract(const char* columnText) const
#ifdef SQLITE_ORM_CPP20_CONCEPTS_SUPPORTED
            requires(orm_column_text_extractable<unqualified_type>)
#endif
        {
            if(columnText) {
                const row_extractor<unqualified_type> rowExtractor{};
                return is_std_ptr<V>::make(rowExtractor.extract(columnText));
            } else {
                return {};
            }
        }

        V extract(sqlite3_stmt* stmt, int columnIndex) const
#ifdef SQLITE_ORM_CPP20_CONCEPTS_SUPPORTED
            requires(orm_row_value_extractable<unqualified_type>)
#endif
        {
            auto type = sqlite3_column_type(stmt, columnIndex);
            if(type != SQLITE_NULL) {
                const row_extractor<unqualified_type> rowExtractor{};
                return is_std_ptr<V>::make(rowExtractor.extract(stmt, columnIndex));
            } else {
                return {};
            }
        }

        V extract(sqlite3_value* value) const
#ifdef SQLITE_ORM_CPP20_CONCEPTS_SUPPORTED
            requires(orm_boxed_value_extractable<unqualified_type>)
#endif
        {
            auto type = sqlite3_value_type(value);
            if(type != SQLITE_NULL) {
                const row_extractor<unqualified_type> rowExtractor{};
                return is_std_ptr<V>::make(rowExtractor.extract(value));
            } else {
                return {};
            }
        }
    };

#ifdef SQLITE_ORM_OPTIONAL_SUPPORTED
    template<class V>
    struct row_extractor<V, std::enable_if_t<polyfill::is_specialization_of_v<V, std::optional>>> {
        using unqualified_type = std::remove_cv_t<typename V::value_type>;

        V extract(const char* columnText) const
#ifdef SQLITE_ORM_CPP20_CONCEPTS_SUPPORTED
            requires(orm_column_text_extractable<unqualified_type>)
#endif
        {
            if(columnText) {
                const row_extractor<unqualified_type> rowExtractor{};
                return std::make_optional(rowExtractor.extract(columnText));
            } else {
                return std::nullopt;
            }
        }

        V extract(sqlite3_stmt* stmt, int columnIndex) const
#ifdef SQLITE_ORM_CPP20_CONCEPTS_SUPPORTED
            requires(orm_row_value_extractable<unqualified_type>)
#endif
        {
            auto type = sqlite3_column_type(stmt, columnIndex);
            if(type != SQLITE_NULL) {
                const row_extractor<unqualified_type> rowExtractor{};
                return std::make_optional(rowExtractor.extract(stmt, columnIndex));
            } else {
                return std::nullopt;
            }
        }

        V extract(sqlite3_value* value) const
#ifdef SQLITE_ORM_CPP20_CONCEPTS_SUPPORTED
            requires(orm_boxed_value_extractable<unqualified_type>)
#endif
        {
            auto type = sqlite3_value_type(value);
            if(type != SQLITE_NULL) {
                const row_extractor<unqualified_type> rowExtractor{};
                return std::make_optional(rowExtractor.extract(value));
            } else {
                return std::nullopt;
            }
        }
    };
#endif  //  SQLITE_ORM_OPTIONAL_SUPPORTED

    template<>
    struct row_extractor<nullptr_t> {
        nullptr_t extract(const char* /*columnText*/) const {
            return nullptr;
        }

        nullptr_t extract(sqlite3_stmt*, int /*columnIndex*/) const {
            return nullptr;
        }

        nullptr_t extract(sqlite3_value*) const {
            return nullptr;
        }
    };
    /**
     *  Specialization for std::vector<char>.
     */
    template<>
    struct row_extractor<std::vector<char>> {
        std::vector<char> extract(const char* columnText) const {
            return {columnText, columnText + (columnText ? ::strlen(columnText) : 0)};
        }

        std::vector<char> extract(sqlite3_stmt* stmt, int columnIndex) const {
            auto bytes = static_cast<const char*>(sqlite3_column_blob(stmt, columnIndex));
            auto len = static_cast<size_t>(sqlite3_column_bytes(stmt, columnIndex));
            return {bytes, bytes + len};
        }

        std::vector<char> extract(sqlite3_value* value) const {
            auto bytes = static_cast<const char*>(sqlite3_value_blob(value));
            auto len = static_cast<size_t>(sqlite3_value_bytes(value));
            return {bytes, bytes + len};
        }
    };

    /**
     *  Specialization for a tuple.
     */
    template<class... Args>
    struct row_extractor<std::tuple<Args...>> {

        std::tuple<Args...> extract(char** argv) const {
            return this->extract(argv, std::make_index_sequence<sizeof...(Args)>{});
        }

        std::tuple<Args...> extract(sqlite3_stmt* stmt, int /*columnIndex*/) const {
            return this->extract(stmt, std::make_index_sequence<sizeof...(Args)>{});
        }

      protected:
        template<size_t... Idx>
        std::tuple<Args...> extract(sqlite3_stmt* stmt, std::index_sequence<Idx...>) const {
            return {row_extractor<Args>{}.extract(stmt, Idx)...};
        }

        template<size_t... Idx>
        std::tuple<Args...> extract(char** argv, std::index_sequence<Idx...>) const {
            return {row_extractor<Args>{}.extract(argv[Idx])...};
        }
    };

    /**
     *  Specialization for journal_mode.
     */
    template<>
    struct row_extractor<journal_mode, void> {
        journal_mode extract(const char* columnText) const {
            if(columnText) {
                if(auto res = internal::journal_mode_from_string(columnText)) {
                    return std::move(*res);
                } else {
                    throw std::system_error{orm_error_code::incorrect_journal_mode_string};
                }
            } else {
                throw std::system_error{orm_error_code::incorrect_journal_mode_string};
            }
        }

        journal_mode extract(sqlite3_stmt* stmt, int columnIndex) const {
            auto cStr = (const char*)sqlite3_column_text(stmt, columnIndex);
            return this->extract(cStr);
        }
    };
}
