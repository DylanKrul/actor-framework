#include <string>
#include <iostream>

#include "caf/all.hpp"

using std::cout;
using std::endl;
using std::string;

using namespace caf;

template <class T>
struct fwrap {
  using fun_type = fwrap (*)(T&, char);

  fun_type f;

  constexpr fwrap(fun_type fun) : f(fun) {
    // nop
  }

  template <class... Us>
  fwrap(char c, const Us&... xs) {
    eval(c, xs...);
  }

  inline fun_type operator()() const {
    return f;
  }

  inline void eval(char) {
    f = nullptr;
  }

  template <class U, class... Us>
  inline void eval(char c, const U& x, const Us&... xs) {
    if (x(c))
      f = x.target();
    else
      eval(c, xs...);
  }
};

template <class T>
using fwrap_fun = fwrap<T> (*)(T&, char);


struct integer_storage {
  bool negative = false;
  int64_t value = 0;
};

integer_storage& reset(integer_storage& x) {
  x.negative = false;
  x.value = 0;
  return x;
}

const char* digit = "0123456789";

template <class T, class F>
struct simple_transition {
  char condition;
  fwrap<T> target;
  F action;
  bool operator()(char c) const {
    if (c == condition) {
      action();
      return true;
    }
    return false;
  }
};

template <class T, class F>
struct strchr_transition {
  const char* condition;
  fwrap<T> target;
  F action;
  bool operator()(char c) const {
    if (c != '\0' && strchr(condition, c) != nullptr) {
      action();
      return true;
    }
    return false;
  }
};

template <class T, class F>
simple_transition<T, F> input(char c, fwrap_fun<T> target, F action) {
  return {c, target, action};
}

template <class T>
simple_transition<T, unit_t> input(char c, fwrap<T> (*target)(T&, char)) {
  return {c, target, unit};
}

template <class T, class F>
strchr_transition<T, F>
input(const char* cstr, fwrap<T> (*target)(T&, char), F action) {
  return {cstr, target, action};
}

template <class T>
strchr_transition<T, unit_t>
input(const char* cstr, fwrap<T> (*target)(T&, char)) {
  return {cstr, target, unit};
}

template <class T, class F>
struct terminal_transition {
  F action;
  fwrap<T> target;
  bool operator()(char c) const {
    if (c == '\0') {
      action();
      return true;
    }
    return false;
  }
};

template <class T, class F>
terminal_transition<T, unit_t> terminal(fwrap<T> (*target)(T&, char), F action) {
  return {action, target};
}

template <class T>
terminal_transition<T, unit_t> terminal(fwrap<T> (*target)(T&, char)) {
  return {unit, target};
}

template <class T>
struct epsilon_transition {
  T& storage;
  mutable fwrap<T> target;
  bool operator()(char c) const {
    target = target.f(storage, c);
    return target.f != nullptr;
  }
};

template <class T>
epsilon_transition<T> epsilon(T& storage, fwrap<T> (*target)(T&, char)) {
  return {storage, target};
}

template <class T>
struct integer {
  using fw = fwrap<T>;

  static fw init(T& x, char c) {
    return {
      c,
      input('-', read_first_digit, [&] { x.negative = true; }),
      epsilon(x, read_first_digit)
    };
  }

  static fw read_first_digit(T& x, char c) {
    return {
      c,
      input(digit, read_digit, [&] { x.value = c - '0'; })
    };
  }

  static fw read_digit(T& x, char c) {
    return {
      c,
      input(digit, read_digit,
                 [&] { x.value = (x.value * 10) + c - '0'; }),
      terminal(fin)
    };
  }

  static fw fin(T&, char) {
    return nullptr;
  }

  static bool is_pre_fin(typename fw::fun_type f) {
    return f == read_digit;
  }
};

template <class T>
struct suffix {
  using fw = fwrap<T>;
  static fw init(T&, char c) {
    return {
      c,
      input('s', at_end)
    };
  }

  static fw at_end(T&, char c) {
    return {
      c,
      terminal(fin)
    };
  }

  static fw fin(T&, char) {
    return nullptr;
  }

  static bool is_pre_fin(typename fw::fun_type f) {
    return f == at_end;
  }
};

template <class T>
struct whitespace {
  using fw = fwrap<T>;
  static fw init(T&, char c) {
    return {
      c,
      input(' ', init),
      terminal(fin)
    };
  }

  static fw fin(T&, char) {
    return nullptr;
  }

  static bool is_pre_fin(typename fw::fun_type f) {
    return f == init;
  }
};

struct parser_tag {};

template <class T>
struct is_parser : std::is_base_of<parser_tag, T> {};

/// A 
template <class Storage, template <class> class Impl>
class single : public parser_tag {
public:
  using impl = Impl<Storage>;
  using storage = Storage;
  using fun = fwrap_fun<storage>;
  using input = impl;
  using output = impl;

  fun init() {
    return impl::init;
  }

  fun epsilon(fun, char) {
    return nullptr;
  }

  bool fin(fun f) {
    return f == impl::fin;
  }
};

template <template <class> class Impl, class T>
single<T, Impl> lift(T&) {
  return {};
}

/// A sequencing decorator for cobining the two parsers `First` and `Second`.
template <class First, class Second>
class sequencing : public parser_tag {
public:
  using storage = typename First::storage;
  static_assert(std::is_same<storage, typename Second::storage>::value,
                "Cannot combine parsers with different storage types.");
  using fun = fwrap_fun<storage>;
  using map_fun = fun (*)(fun, storage&, char);

  using input = typename First::output;
  using output = typename Second::input;

  sequencing(First p0, Second p1, map_fun m) : p0_(p0), p1_(p1), m_(m) {
    // nop
  }

  fun init() {
    return p0_.init();
  }

  fun epsilon(fun from, storage& x, char c) {
    return m_(from, x, c);
  }

  bool fin(fun f) {
    return second().fin(f);
  }

  First& first() {
    return p0_;
  }

  Second& second() {
    return p1_;
  }

private:
  First p0_;
  Second p1_;
  map_fun m_;
};

template <class Storage, class First, class Second>
fwrap_fun<Storage> default_parser_mapping(fwrap_fun<Storage> f, Storage& x, char c) {
  if (First::is_pre_fin(f)) {
    auto g = Second::init(x, c).f;
    if (g != nullptr) {
      // Tell first parser it reached the end.
      auto fin = f(x, '\0');
      CAF_ASSERT(fin.f == First::fin);
      CAF_IGNORE_UNUSED(fin);
      return g;
    }
  }
  return nullptr;
}

template <class First, class Second,
          class E = detail::enable_if_t<is_parser<First>::value
                                        && is_parser<Second>::value>>
sequencing<First, Second>
operator*(First x, Second y) {
  return {
    x, y,
    default_parser_mapping<typename First::storage, typename First::output,
                           typename Second::input>};
}

template <class Storage, template <class> class Impl, class Iterator>
bool parse(single<Storage, Impl>& p, fwrap_fun<Storage> f, Storage& x,
           Iterator i, Iterator e) {
  while (i != e && f != nullptr)
    f = f(x, *i++).f;
  if (i == e && f != nullptr)
    return p.fin(f(x, '\0').f);
  return false;
}

template <class Storage, template <class> class Impl, class Iterator>
bool parse(single<Storage, Impl>& p, Storage& x, Iterator i, Iterator e) {
  return parse(p, p.init(), x, i, e);
}

template <class Storage, class First, class Second, class Iterator>
bool parse(sequencing<First, Second>& p, fwrap_fun<Storage> f,Storage& x, Iterator i, Iterator e) {
  while (i != e && f != nullptr) {
    auto g = f;
    f = g(x, *i).f;
    if (f == nullptr) {
      f = p.epsilon(g, x, *i);
      if (f != nullptr)
        return parse(p.second(), f, x, ++i, e);
      else
        return false;
    }
    ++i;
  }
  if (i == e && f != nullptr) {
    // Tell first parser it reached the end.
    if (!p.first().fin(f(x, '\0').f))
      return false;
    // Check whether second parser accepts an empty input.
    return parse(p.second(), p.second().init(), x, e, e);
  }
  return false;
}

template <class Storage, class First, class Second, class Iterator>
bool parse(sequencing<First, Second>& p, Storage& x, Iterator i, Iterator e) {
  return parse(p, p.init(), x, i, e);
}

template <class Parser>
bool parse(Parser& p, typename Parser::storage& x, const std::string& str) {
  return parse(p, x, str.begin(), str.end());
}

int main() {
  integer_storage st;
  // build an integer parser
  auto ip = lift<integer>(st);
  cout << std::boolalpha;
  cout << "'1': " << parse(ip, reset(st), "1") << " -> " << st.value << endl;
  cout << "'123': " << parse(ip, reset(st), "123") << " -> " << st.value << endl;
  cout << "'0xFF': " << parse(ip, reset(st), "0xFF") << " -> " << st.value << endl;
  cout << "'0xFF': " << parse(ip, reset(st), "0xFF") << " -> " << st.value << endl;
  cout << "'123s': " << parse(ip, reset(st), "123s") << " -> " << st.value << endl;
  /*
  using fun = fwrap_fun<integer_storage>;
  auto mapping = [](fun from, integer_storage& x, char c) -> fun {
    if (from == integer<integer_storage>::read_digit)
      return suffix<integer_storage>::init(x, c).f;
    return nullptr;
  };
  // duration parser
  auto dp = sequence<suffix>(ip, mapping);
  */
  auto sx = lift<suffix>(st);
  auto dp = ip * sx;
  cout << "'123s': " << parse(dp, reset(st), "123s") << " -> " << st.value << endl;
  auto ws = lift<whitespace>(st);
  cout << "--- " << endl;
  {
    auto parse = [&](const std::string& str) {
      cout << "'" << str << "': " << ::parse(ws, reset(st), str) << endl;
    };
    cout << "ws: " << endl;
    parse("1");
    parse("");
    parse(" ");
    parse("     ");
    parse("        ");
    parse("   ");
  }
  cout << "--- " << endl;
  {
    auto p = ws * ip;
    auto parse = [&](const std::string& str) {
      cout << "'" << str << "': " << ::parse(p, reset(st), str) << " -> "
           << st.value << endl;
    };
    cout << "ws * ip: " << endl;
    parse("1");
    parse("     1");
    parse("     1   ");
    parse("1   ");
  }
  cout << "--- " << endl;
  {
    auto p = ip * ws;
    auto parse = [&](const std::string& str) {
      cout << "'" << str << "': " << ::parse(p, reset(st), str) << " -> "
           << st.value << endl;
    };
    cout << "ip * ws: " << endl;
    parse("1");
    parse("     1");
    parse("     1   ");
    parse("1   ");
  }
  cout << "--- " << endl;
  {
    auto p = (ws * ip) * ws;
    auto parse = [&](const std::string& str) {
      cout << "'" << str << "': " << ::parse(p, reset(st), str) << " -> "
           << st.value << endl;
    };
    cout << "(ws * ip) * ws: " << endl;
    parse("1");
    parse("     1");
    parse("     1   ");
    parse("1   ");
  }
  {
    auto p = ws * (ip * ws);
    auto parse = [&](const std::string& str) {
      cout << "'" << str << "': " << ::parse(p, reset(st), str) << " -> "
           << st.value << endl;
    };
    cout << "ws * (ip * ws): " << endl;
    parse("1");
    parse("     1");
    parse("     1   ");
    parse("1   ");
  }
}
