#include <iostream>
#include <boost/preprocessor.hpp>
#include <utility>
#include <type_traits>
#include <sstream>
#include <json/json.h>
#include <vector>
#define REM(...) __VA_ARGS__
#define EAT(...)
// Strip off the type
#define STRIP(x) EAT x
// Show the type without parenthesis
#define PAIR(x) REM x
// A helper metafunction for adding const to a type
template<class M, class T>
struct make_const
{
    typedef T type;
};
template<class M, class T>
struct make_const<const M, T>
{
    using type = typename std::add_const<T>::type;
};

#define REFLECTABLE(...) \
static const int fields_n = BOOST_PP_VARIADIC_SIZE(__VA_ARGS__); \
friend struct reflector; \
template<int N, class Self> \
struct field_data {}; \
BOOST_PP_SEQ_FOR_EACH_I(REFLECT_EACH, data, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))
#define REFLECT_EACH(r, data, i, x) \
PAIR(x); \
template<class Self> \
struct field_data<i, Self> \
{ \
    Self & self; \
    field_data(Self & self) : self(self) {} \
    \
    auto & get() \
    { \
        return self.STRIP(x); \
    }\
    auto & get() const \
    { \
        return self.STRIP(x); \
    }\
    const char* name() const \
    {\
        return BOOST_PP_STRINGIZE(STRIP(x)); \
    } \
};

struct reflector
{
    //Get field_data at index N
    template<int N, class T>
    static typename T::template field_data<N, T> get_field_data(T& x)
    {
        return typename T::template field_data<N, T>(x);
    }
    // Get the number of fields
    template<class T>
    struct fields
    {
        static const int n = T::fields_n;
    };
};
namespace
{
    template<class C, class Visitor, std::size_t...Is>
    void visit_each(C & c, Visitor v, std::index_sequence<Is...>)
    {
        (v(reflector::get_field_data<Is>(c).name(), reflector::get_field_data<Is>(c).get()), ...);
    }
    
}
template<class C, class Visitor>
void visit_each(C & c, Visitor v)
{
    visit_each(c, v, std::make_index_sequence<reflector::fields<C>::n>{});
}
template<class T>
Json::Value print_fields(T & x);

template<typename S, typename T>
class is_streamable
{
    template<typename SS, typename TT>
    static auto test(SS&& s, TT&& t) -> decltype(s << t);

    struct dummy_t {};
    static dummy_t test(...);

    using return_type = decltype(test(std::declval<S>(), std::declval<T>()));

public:
    static const bool value = !std::is_same<return_type, dummy_t>::value;
};

struct print_visitor
{
    print_visitor(Json::Value &value) : value(value) {}
    Json::Value &value;

    template<class Data, std::enable_if_t<is_streamable<std::stringstream, Data>::value> *x = nullptr>
    void operator()(const char *name, const Data &f)
    {
        value[name] = f;
    }
    template<class Data, std::enable_if_t<not is_streamable<std::stringstream, Data>::value> *x = nullptr>
    void operator()(const char *name, const Data &f)
    {
        value[name] = print_fields(f);
    }
    template<class Data, std::enable_if_t<is_streamable<std::stringstream, Data>::value> *x = nullptr>
    void operator()(const char *name, const std::vector<Data> &vf)
    {
        for (auto f : vf) 
            value[name].append(f);
    }
    template<class Data, std::enable_if_t<not is_streamable<std::stringstream, Data>::value> *x = nullptr>
    void operator()(const char *name, const std::vector<Data> &vf)
    {
        for (auto f : vf) 
            value[name].append(print_fields(f));
    }
};

template<class T>
Json::Value print_fields(T & x)
{
    Json::Value value;
    visit_each(x, print_visitor(value));
    return value;
}


struct Person
{
    Person(const char *name, int age)
        :
        name(name),
        age(age)
    {
    }
private:
    REFLECTABLE
    (
        (const char *) name,
        (int) age
    )
};

struct Group {

      Group(Person p1, Person p2, Person p3): vp{p1, p2, p3}, vi{23, 45} {}
private:
      REFLECTABLE
      (
      (std::vector<Person>) vp,
      (std::vector<int>) vi
      )
};

int main()
{
    Person p1("Tom", 82);
    Person p2("Sam", 45);
    Person p3("Max", 38);
    const Group g(p1, p2, p3);
    std::cout << print_fields(g) << std::endl;
}
