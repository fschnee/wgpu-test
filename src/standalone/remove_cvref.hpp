#pragma once

namespace standalone
{
    template <typename T> struct remove_ref      { using type = T; };
    template <typename T> struct remove_ref<T&>  { using type = T; };
    template <typename T> struct remove_ref<T&&> { using type = T; };
    template <typename T> using  remove_ref_t = typename remove_ref<T>::type;

    template <typename T> struct remove_cv                   { using type = T; };
    template <typename T> struct remove_cv<const T>          { using type = T; };
    template <typename T> struct remove_cv<volatile T>       { using type = T; };
    template <typename T> struct remove_cv<const volatile T> { using type = T; };
    template <typename T> using  remove_cv_t = typename remove_cv<T>::type;

    template <typename T> struct remove_cvref { using type = remove_cv_t< remove_ref_t<T> >; };
    template <typename T> using  remove_cvref_t = typename remove_cvref<T>::type;
}