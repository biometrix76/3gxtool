/*

Copyright (c) 2014, 2015, 2016, 2017 Jarryd Beck

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

#ifndef CXXOPTS_HPP_INCLUDED
#define CXXOPTS_HPP_INCLUDED

#include <cstring>
#include <cctype>
#include <exception>
#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef __cpp_lib_optional
#include <optional>
#define CXXOPTS_HAS_OPTIONAL
#endif

using namespace std;

namespace cxxopts {
  static constexpr struct {
    uint8_t major, minor, patch;
  } version = {2, 1, 0};
}

#ifdef CXXOPTS_USE_UNICODE
#include <unicode/unistr.h>

namespace cxxopts {
  typedef icu::UnicodeString String;
  inline String toLocalString(string s) {
    return icu::UnicodeString::fromUTF8(std::move(s));
  }

  class UnicodeStringIterator : public iterator<std::forward_iterator_tag, int32_t> {
    public:
    UnicodeStringIterator(const icu::UnicodeString* string, int32_t pos)
    : s(string), i(pos) {}

    value_type operator*() const {
      return s->char32At(i);
    }

    bool operator==(const UnicodeStringIterator &rhs) const {
      return s == rhs.s && i == rhs.i;
    }

    bool operator!=(const UnicodeStringIterator &rhs) const {
      return !(*this == rhs);
    }

    UnicodeStringIterator &operator++() {
      ++i;
      return *this;
    }

    UnicodeStringIterator operator+(int32_t v) {
      return UnicodeStringIterator(s, i + v);
    }

    private:
    const icu::UnicodeString *s;
    int32_t i;
  };

  inline String &stringAppend(String &s, String a) {
    return s.append(std::move(a));
  }

  inline String &stringAppend(String &s, int n, UChar32 c) {
    for (int i = 0; i != n; ++i)
      s.append(c);

    return s;
  }

  template <typename Iterator> String &stringAppend(String &s, Iterator begin, Iterator end) {
    while (begin != end) {
      s.append(*begin);
      ++begin;
    }

    return s;
  }

  inline size_t stringLength(const String &s) {
    return s.length();
  }

  inline string std::toUTF8String(const String &s) {
    string result;
    s.toUTF8String(result);
    return result;
  }

  inline bool empty(const String &s) {
    return s.isEmpty();
  }
}

namespace std {
  inline cxxopts::UnicodeStringIterator begin(const icu::UnicodeString &s) {
    return cxxopts::UnicodeStringIterator(&s, 0);
  }

  inline cxxopts::UnicodeStringIterator end(const icu::UnicodeString &s) {
    return cxxopts::UnicodeStringIterator(&s, s.length());
  }
}

#else

namespace cxxopts {
  typedef string String;

  template <typename T>
  T toLocalString(T &&t) {
    return t;
  }

  inline size_t stringLength(const String &s) {
    return s.length();
  }

  inline String &stringAppend(String &s, String a) {
    return s.append(std::move(a));
  }

  inline String &stringAppend(String &s, size_t n, char c) {
    return s.append(n, c);
  }

  template <typename Iterator> String &stringAppend(String &s, Iterator begin, Iterator end) {
    return s.append(begin, end);
  }

  template <typename T> string toUTF8String(T &&t) {
    return std::forward<T>(t);
  }

  inline bool empty(const string &s) {
    return s.empty();
  }
}

#endif

namespace cxxopts {
  namespace {
#ifdef _WIN32
    const string LQUOTE("\'");
    const string RQUOTE("\'");
#else
    const string LQUOTE("‘");
    const string RQUOTE("’");
#endif
  }

  class Value : public enable_shared_from_this<Value> {
    public:
    virtual ~Value() = default;
    virtual shared_ptr<Value> clone() const = 0;
    virtual void parse(const string &text) const = 0;
    virtual void parse() const = 0;
    virtual bool has_default() const = 0;
    virtual bool is_container() const = 0;
    virtual bool has_implicit() const = 0;
    virtual string get_default_value() const = 0;
    virtual string get_implicit_value() const = 0;
    virtual shared_ptr<Value> default_value(const string &value) = 0;
    virtual shared_ptr<Value> implicit_value(const string &value) = 0;
    virtual bool is_boolean() const = 0;
  };

  class OptionException : public exception {
    public:
    OptionException(const string &message)
    : m_message(message) {}

    virtual const char *what() const noexcept {
      return m_message.c_str();
    }

    private:
    string m_message;
  };

  class OptionSpecException : public OptionException {
    public:

    OptionSpecException(const string &message)
    : OptionException(message) {}
  };

  class OptionParseException : public OptionException {
    public:
    OptionParseException(const string &message)
    : OptionException(message) {}
  };

  class option_exists_error : public OptionSpecException {
    public:
    option_exists_error(const string &option)
    : OptionSpecException(u8"Option " + LQUOTE + option + RQUOTE + u8" already exists") {}
  };

  class invalid_option_format_error : public OptionSpecException {
    public:
    invalid_option_format_error(const string &format)
    : OptionSpecException(u8"Invalid option format " + LQUOTE + format + RQUOTE) {}
  };

  class option_not_exists_exception : public OptionParseException {
    public:
    option_not_exists_exception(const string &option)
    : OptionParseException(u8"Option " + LQUOTE + option + RQUOTE + u8" does not exist") {}
  };

  class missing_argument_exception : public OptionParseException {
    public:
    missing_argument_exception(const string &option)
    : OptionParseException(
        u8"Option " + LQUOTE + option + RQUOTE + u8" is missing an argument"
      ) {}
  };

  class option_requires_argument_exception : public OptionParseException {
    public:
    option_requires_argument_exception(const string &option)
    : OptionParseException(
        u8"Option " + LQUOTE + option + RQUOTE + u8" requires an argument"
      ) {}
  };

  class option_not_has_argument_exception : public OptionParseException {
    public:
    option_not_has_argument_exception (
      const string &option,
      const string &arg
    )
    : OptionParseException(
        u8"Option " + LQUOTE + option + RQUOTE +
        u8" does not take an argument, but argument " +
        LQUOTE + arg + RQUOTE + " given"
      ) {}
  };

  class option_not_present_exception : public OptionParseException {
    public:
    option_not_present_exception(const string &option)
    : OptionParseException(u8"Option " + LQUOTE + option + RQUOTE + u8" not present") {}
  };

  class argument_incorrect_type : public OptionParseException {
    public:
    argument_incorrect_type(
      const string &arg
    )
    : OptionParseException(
        u8"Argument " + LQUOTE + arg + RQUOTE + u8" failed to parse"
      ) {}
  };

  class option_required_exception : public OptionParseException {
    public:
    option_required_exception(const string &option)
    : OptionParseException(
        u8"Option " + LQUOTE + option + RQUOTE + u8" is required but not present"
      ) {}
  };

  namespace values {
    namespace {
      basic_regex<char> integer_pattern
        ("(-)?(0x)?([1-9a-zA-Z][0-9a-zA-Z]*)|((0x)?0)");
      basic_regex<char> truthy_pattern
        ("(t|T)(rue)?");
      basic_regex<char> falsy_pattern
        ("((f|F)(alse)?)?");
    }

    namespace detail {
      template <typename T, bool B>
      struct SignedCheck;

      template <typename T>
      struct SignedCheck<T, true> {
        template <typename U>
        void operator()(bool negative, U u, const string &text) {
          if (negative) {
            if (u > static_cast<U>(-numeric_limits<T>::min()))
              throw argument_incorrect_type(text);
          }

          else {
            if (u > static_cast<U>(numeric_limits<T>::max()))
              throw argument_incorrect_type(text);
          }
        }
      };

      template <typename T>
      struct SignedCheck<T, false> {
        template <typename U>
        void
        operator()(bool, U, const string&) {}
      };

      template <typename T, typename U>
      void check_signed_range(bool negative, U value, const string &text) {
        SignedCheck<T, numeric_limits<T>::is_signed>()(negative, value, text);
      }
    }

    template <typename R, typename T>
    R checked_negate(T&& t, const string&, true_type) {
      return -static_cast<R>(t);
    }

    template <typename R, typename T>
    T checked_negate(T&&, const string &text, false_type) {
      throw argument_incorrect_type(text);
    }

    template <typename T>
    void integer_parser(const string &text, T &value) {
      smatch match;
      regex_match(text, match, integer_pattern);

      if (match.length() == 0)
        throw argument_incorrect_type(text);


      if (match.length(4) > 0) {
        value = 0;
        return;
      }

      using US = typename make_unsigned<T>::type;
      constexpr auto umax = numeric_limits<US>::max();
      constexpr bool is_signed = numeric_limits<T>::is_signed;
      const bool negative = match.length(1) > 0;
      const uint8_t base = match.length(2) > 0 ? 16 : 10;
      auto value_match = match[3];
      US result = 0;

      for (auto iter = value_match.first; iter != value_match.second; ++iter) {
        size_t digit = 0;

        if (*iter >= '0' && *iter <= '9')
          digit = *iter - '0';

        else if (base == 16 && *iter >= 'a' && *iter <= 'f')
          digit = *iter - 'a' + 10;

        else if (base == 16 && *iter >= 'A' && *iter <= 'F')
          digit = *iter - 'A' + 10;

        else throw argument_incorrect_type(text);

        if (umax - digit < result * base)
          throw argument_incorrect_type(text);

        result = result * base + digit;
      }

      detail::check_signed_range<T>(negative, result, text);

      if (negative) {
        value = checked_negate<T>(result,
          text,
          integral_constant<bool, is_signed>());
      }

      else value = result;
    }

    template <typename T>
    void stringstream_parser(const string &text, T &value) {
      stringstream in(text);
      in >> value;
      if (!in)
        throw argument_incorrect_type(text);
    }

    inline void parse_value(const string &text, uint8_t &value) {
      integer_parser(text, value);
    }

    inline void parse_value(const string &text, int8_t &value) {
      integer_parser(text, value);
    }

    inline void parse_value(const string &text, uint16_t &value) {
      integer_parser(text, value);
    }

    inline void parse_value(const string &text, int16_t &value) {
      integer_parser(text, value);
    }

    inline void parse_value(const string &text, uint32_t &value) {
      integer_parser(text, value);
    }

    inline void parse_value(const string &text, int32_t &value) {
      integer_parser(text, value);
    }

    inline void parse_value(const string &text, uint64_t &value) {
      integer_parser(text, value);
    }

    inline void parse_value(const string &text, int64_t &value) {
      integer_parser(text, value);
    }

    inline void parse_value(const string &text, bool &value) {
      smatch result;
      regex_match(text, result, truthy_pattern);

      if (!result.empty()) {
        value = true;
        return;
      }

      regex_match(text, result, falsy_pattern);

      if (!result.empty()) {
        value = false;
        return;
      }

      throw argument_incorrect_type(text);
    }

    inline void parse_value(const string &text, string &value) {
      value = text;
    }

    template <typename T>
    void parse_value(const string &text, T &value) {
      stringstream_parser(text, value);
    }

    template <typename T>
    void parse_value(const string &text, vector<T> &value) {
      T v;
      parse_value(text, v);
      value.push_back(v);
    }

#ifdef CXXOPTS_HAS_OPTIONAL
    template <typename T>
    void parse_value(const string &text, optional<T> &value) {
      T result;
      parse_value(text, result);
      value = std::move(result);
    }
#endif

    template <typename T>
    struct type_is_container {
      static constexpr bool value = false;
    };

    template <typename T>
    struct type_is_container<vector<T>> {
      static constexpr bool value = true;
    };

    template <typename T>
    class abstract_value : public Value {
      using Self = abstract_value<T>;

      public:
      abstract_value()
      : m_result(make_shared<T>())
      , m_store(m_result.get())
      {}

      abstract_value(T* t)
      : m_store(t)
      {}

      virtual ~abstract_value() = default;

      abstract_value(const abstract_value &rhs) {
        if (rhs.m_result) {
          m_result = make_shared<T>();
          m_store = m_result.get();
        }

        else m_store = rhs.m_store;

        m_default = rhs.m_default;
        m_implicit = rhs.m_implicit;
        m_default_value = rhs.m_default_value;
        m_implicit_value = rhs.m_implicit_value;
      }

      void parse(const string &text) const {
        parse_value(text, *m_store);
      }

      bool is_container() const {
        return type_is_container<T>::value;
      }

      void parse() const {
        parse_value(m_default_value, *m_store);
      }

      bool has_default() const {
        return m_default;
      }

      bool has_implicit() const {
        return m_implicit;
      }

      shared_ptr<Value>
      default_value(const string &value) {
        m_default = true;
        m_default_value = value;
        return shared_from_this();
      }

      shared_ptr<Value>
      implicit_value(const string &value) {
        m_implicit = true;
        m_implicit_value = value;
        return shared_from_this();
      }

      string get_default_value() const {
        return m_default_value;
      }

      string get_implicit_value() const {
        return m_implicit_value;
      }

      bool is_boolean() const {
        return is_same<T, bool>::value;
      }

      const T &get() const {
        if (m_store == nullptr)
          return *m_result;

        else return *m_store;
      }

      protected:
      shared_ptr<T> m_result;
      T *m_store;

      bool m_default = false;
      bool m_implicit = false;

      string m_default_value;
      string m_implicit_value;
    };

    template <typename T>
    class standard_value : public abstract_value<T> {
      public:
      using abstract_value<T>::abstract_value;

      shared_ptr<Value>
      clone() const {
        return make_shared<standard_value<T>>(*this);
      }
    };

    template <>
    class standard_value<bool> : public abstract_value<bool> {
      public:
      ~standard_value() = default;

      standard_value() {
        set_default_and_implicit();
      }

      standard_value(bool *b)
      : abstract_value(b) {
        set_default_and_implicit();
      }

      shared_ptr<Value>
      clone() const {
        return make_shared<standard_value<bool>>(*this);
      }

      private:
      void set_default_and_implicit() {
        m_default = true;
        m_default_value = "false";
        m_implicit = true;
        m_implicit_value = "true";
      }
    };
  }

  template <typename T>
  shared_ptr<Value> value() {
    return make_shared<values::standard_value<T>>();
  }

  template <typename T>
  shared_ptr<Value> value(T &t) {
    return make_shared<values::standard_value<T>>(&t);
  }

  class OptionAdder;

  class OptionDetails {
    public:
    OptionDetails (
      const string &short_,
      const string &long_,
      const string &desc,
      shared_ptr<const Value> val
    )

    : m_short(short_)
    , m_long(long_)
    , m_desc(desc)
    , m_value(val)
    , m_count(0)
    {}

    OptionDetails(const OptionDetails &rhs)
    : m_desc(rhs.m_desc)
    , m_count(rhs.m_count) {
      m_value = rhs.m_value->clone();
    }

    OptionDetails(OptionDetails &&rhs) = default;

    const String & description() const {
      return m_desc;
    }

    const Value &value() const {
        return *m_value;
    }

    shared_ptr<Value> make_storage() const {
      return m_value->clone();
    }

    const string &short_name() const {
      return m_short;
    }

    const string & long_name() const {
      return m_long;
    }

    private:
    string m_short;
    string m_long;
    String m_desc;
    shared_ptr<const Value> m_value;
    int m_count;
  };

  struct HelpOptionDetails {
    string s;
    string l;
    String desc;
    bool has_default;
    string default_value;
    bool has_implicit;
    string implicit_value;
    string arg_help;
    bool is_container;
    bool is_boolean;
  };

  struct HelpGroupDetails {
    string name;
    string description;
    vector<HelpOptionDetails> options;
  };

  class OptionValue {
    public:
    void parse (shared_ptr<const OptionDetails> details, const string &text) {
      ensure_value(details);
      ++m_count;
      m_value->parse(text);
    }

    void parse_default(shared_ptr<const OptionDetails> details) {
      ensure_value(details);
      m_value->parse();
    }

    size_t count() const {
      return m_count;
    }

    template <typename T>
    const T &as() const {
#ifdef CXXOPTS_NO_RTTI
      return static_cast<const values::standard_value<T>&>(*m_value).get();
#else
      return dynamic_cast<const values::standard_value<T>&>(*m_value).get();
#endif
    }

    private:
    void ensure_value(shared_ptr<const OptionDetails> details) {
      if (m_value == nullptr) {
        m_value = details->make_storage();
      }
    }

    shared_ptr<Value> m_value;
    size_t m_count = 0;
  };

  class KeyValue {
    public:
    KeyValue(string key_, string value_)
    : m_key(std::move(key_))
    , m_value(std::move(value_))
    {}

    const string &key() const {
      return m_key;
    }

    const string value() const {
      return m_value;
    }

    template <typename T>
    T as() const {
      T result;
      values::parse_value(m_value, result);
      return result;
    }

    private:
    string m_key;
    string m_value;
  };

  class ParseResult {
    public:
    ParseResult(const unordered_map<string, shared_ptr<OptionDetails>>&, vector<string>, int&, const char**&);

    size_t count(const string &o) const {
      auto iter = m_options.find(o);
      if (iter == m_options.end())
        return 0;

      auto riter = m_results.find(iter->second);
      return riter->second.count();
    }

    const OptionValue &operator[](const string &option) const {
      auto iter = m_options.find(option);
      if (iter == m_options.end())
        throw option_not_present_exception(option);

      auto riter = m_results.find(iter->second);
      return riter->second;
    }

    const vector<KeyValue> &arguments() const {
      return m_sequential;
    }

    private:
    OptionValue &get_option(shared_ptr<OptionDetails>);
    void parse(int &argc, const char**& argv);
    void add_to_option(const string &option, const string &arg);
    bool consume_positional(string a);
    void parse_option(shared_ptr<OptionDetails> value, const string &name, const string &arg = "");
    void parse_default(shared_ptr<OptionDetails> details);
    void checked_parse_arg (int argc, const char *argv[], int &current, shared_ptr<OptionDetails> value, const string &name);
    const unordered_map<string, shared_ptr<OptionDetails>>
    &m_options;
    vector<string> m_positional;
    vector<string>::iterator m_next_positional;
    unordered_set<string> m_positional_set;
    unordered_map<shared_ptr<OptionDetails>, OptionValue> m_results;
    vector<KeyValue> m_sequential;
  };

  class Options {
    public:
    Options(string program, string help_string = "")
    : m_program(std::move(program))
    , m_help_string(toLocalString(std::move(help_string)))
    , m_custom_help("[OPTION...]")
    , m_positional_help("positional parameters")
    , m_show_positional(false)
    , m_next_positional(m_positional.end()) {
    }

    Options &positional_help(string help_text) {
      m_positional_help = std::move(help_text);
      return *this;
    }

    Options &custom_help(string help_text) {
      m_custom_help = std::move(help_text);
      return *this;
    }

    Options &show_positional_help() {
      m_show_positional = true;
      return *this;
    }

    ParseResult parse(int &argc, const char **&argv);

    OptionAdder add_options(string group = "");

    void add_option (
      const string &group,
      const string &s,
      const string &l,
      string desc,
      shared_ptr<const Value> value,
      string arg_help
    );

    void parse_positional(string option);
    void parse_positional(vector<string> options);
    void parse_positional(initializer_list<string> options);
    string help(const vector<string> &groups = {""}) const;
    const vector<string> groups() const;
    const HelpGroupDetails &group_help(const string &group) const;

    private:
    void add_one_option (const string &option, shared_ptr<OptionDetails> details);
    String help_one_group(const string &group) const;
    void generate_group_help (string &result, const vector<string> &groups) const;
    void generate_all_groups_help(string &result) const;

    string m_program;
    String m_help_string;
    string m_custom_help;
    string m_positional_help;
    bool m_show_positional;

    unordered_map<string, shared_ptr<OptionDetails>> m_options;
    vector<string> m_positional;
    vector<string>::iterator m_next_positional;
    unordered_set<string> m_positional_set;

    map<string, HelpGroupDetails> m_help;
  };

  class OptionAdder {
    public:

    OptionAdder(Options &options, string group)
    : m_options(options), m_group(std::move(group))
    {}

    OptionAdder &operator() (
      const string &opts,
      const string &desc,
      shared_ptr<const Value> value
        = ::cxxopts::value<bool>(),
      string arg_help = ""
    );

    private:
    Options &m_options;
    string m_group;
  };

  namespace {
    constexpr int OPTION_LONGEST = 30;
    constexpr int OPTION_DESC_GAP = 2;

    basic_regex<char> option_matcher
      ("--([[:alnum:]][-_[:alnum:]]+)(=(.*))?|-([[:alnum:]]+)");

    basic_regex<char> option_specifier
      ("(([[:alnum:]]),)?[ ]*([[:alnum:]][-_[:alnum:]]*)?");

    String format_option(
      const HelpOptionDetails &o
    ) {
      auto &s = o.s;
      auto &l = o.l;

      String result = "  ";

      if (s.size() > 0)
        result += "-" + toLocalString(s) + ",";

      else result += "   ";

      if (l.size() > 0)
        result += " --" + toLocalString(l);

      auto arg = o.arg_help.size() > 0 ? toLocalString(o.arg_help) : "arg";

      if (!o.is_boolean) {
        if (o.has_implicit)
          result += " [=" + arg + "(=" + toLocalString(o.implicit_value) + ")]";

        else result += " " + arg;
      }

      return result;
    }

    String format_description (
      const HelpOptionDetails &o,
      size_t start,
      size_t width
    ) {
      auto desc = o.desc;

      if (o.has_default && (!o.is_boolean || o.default_value != "false"))
        desc += toLocalString(" (default: " + o.default_value + ")");

      String result;
      auto current = begin(desc);
      auto startLine = current;
      auto lastSpace = current;
      auto size = size_t{};

      while (current != end(desc)) {
        if (*current == ' ')
          lastSpace = current;

        if (size > width) {
          if (lastSpace == startLine) {
            stringAppend(result, startLine, current + 1);
            stringAppend(result, "\n");
            stringAppend(result, start, ' ');
            startLine = current + 1;
            lastSpace = startLine;
          }

          else {
            stringAppend(result, startLine, lastSpace);
            stringAppend(result, "\n");
            stringAppend(result, start, ' ');
            startLine = lastSpace + 1;
          }

          size = 0;
        }

        else ++size;
        ++current;
      }

      stringAppend(result, startLine, current);
      return result;
    }
  }

inline ParseResult::ParseResult (
  const unordered_map<string, shared_ptr<OptionDetails>> &options,
  vector<string> positional,
  int &argc, const char **&argv
)
: m_options(options)
, m_positional(std::move(positional))
, m_next_positional(m_positional.begin()) {
  parse(argc, argv);
}

inline OptionAdder Options::add_options(string group) {
  return OptionAdder(*this, std::move(group));
}

inline OptionAdder &OptionAdder::operator() (
  const string &opts,
  const string &desc,
  shared_ptr<const Value> value,
  string arg_help
) {
  match_results<const char*> result;
  regex_match(opts.c_str(), result, option_specifier);

  if (result.empty())
    throw invalid_option_format_error(opts);

  const auto &short_match = result[2];
  const auto &long_match = result[3];

  if (!short_match.length() && !long_match.length())
    throw invalid_option_format_error(opts);

  else if (long_match.length() == 1 && short_match.length())
    throw invalid_option_format_error(opts);

  auto option_names = [] (
    const sub_match<const char*> &short_,
    const sub_match<const char*> &long_
  ) {
    if (long_.length() == 1)
      return make_tuple(long_.str(), short_.str());

    else return make_tuple(short_.str(), long_.str());

  }(short_match, long_match);

  m_options.add_option (
    m_group,
    get<0>(option_names),
    get<1>(option_names),
    desc,
    value,
    std::move(arg_help)
  );

  return *this;
}

inline void ParseResult::parse_default(shared_ptr<OptionDetails> details) {
  m_results[details].parse_default(details);
}

inline void ParseResult::parse_option (
  shared_ptr<OptionDetails> value,
  const string &/*name*/,
  const string &arg
) {
  auto &result = m_results[value];
  result.parse(value, arg);
  m_sequential.emplace_back(value->long_name(), arg);
}

inline void ParseResult::checked_parse_arg (
  int argc,
  const char *argv[],
  int &current,
  shared_ptr<OptionDetails> value,
  const string &name
) {
  if (current + 1 >= argc) {
    if (value->value().has_implicit())
      parse_option(value, name, value->value().get_implicit_value());

    else throw missing_argument_exception(name);
  }

  else {
    if (value->value().has_implicit())
      parse_option(value, name, value->value().get_implicit_value());

    else {
      parse_option(value, name, argv[current + 1]);
      ++current;
    }
  }
}

inline void ParseResult::add_to_option(const string &option, const string &arg) {
  auto iter = m_options.find(option);

  if (iter == m_options.end())
    throw option_not_exists_exception(option);

  parse_option(iter->second, option, arg);
}

inline bool ParseResult::consume_positional(string a) {
  while (m_next_positional != m_positional.end()) {
    auto iter = m_options.find(*m_next_positional);

    if (iter != m_options.end()) {
      auto &result = m_results[iter->second];

      if (!iter->second->value().is_container()) {
        if (result.count() == 0) {
          add_to_option(*m_next_positional, a);
          ++m_next_positional;
          return true;
        }

        else {
          ++m_next_positional;
          continue;
        }
      }

      else {
        add_to_option(*m_next_positional, a);
        return true;
      }
    }

    ++m_next_positional;
  }

  return false;
}

inline void Options::parse_positional(string option) {
  parse_positional(vector<string>{std::move(option)});
}

inline void Options::parse_positional(vector<string> options) {
  m_positional = std::move(options);
  m_next_positional = m_positional.begin();
  m_positional_set.insert(m_positional.begin(), m_positional.end());
}

inline void Options::parse_positional(initializer_list<string> options) {
  parse_positional(vector<string>(std::move(options)));
}

inline ParseResult Options::parse(int &argc, const char **&argv) {
  ParseResult result(m_options, m_positional, argc, argv);
  return result;
}

inline void ParseResult::parse(int &argc, const char **&argv) {
  int current = 1;
  int nextKeep = 1;
  bool consume_remaining = false;

  while (current != argc) {
    if (strcmp(argv[current], "--") == 0) {
      consume_remaining = true;
      ++current;
      break;
    }

    match_results<const char*> result;
    regex_match(argv[current], result, option_matcher);

    if (result.empty()) {
      if (consume_positional(argv[current])) {}

      else {
        argv[nextKeep] = argv[current];
        ++nextKeep;
      }
    }

    else {
      if (result[4].length() != 0) {
        const string &s = result[4];

        for (size_t i = 0; i != s.size(); ++i) {
          string name(1, s[i]);
          auto iter = m_options.find(name);

          if (iter == m_options.end())
            throw option_not_exists_exception(name);

          auto value = iter->second;

          if (i + 1 == s.size())
            checked_parse_arg(argc, argv, current, value, name);

          else if (value->value().has_implicit())
            parse_option(value, name, value->value().get_implicit_value());

          else throw option_requires_argument_exception(name);
        }
      }

      else if (result[1].length() != 0) {
        const string &name = result[1];
        auto iter = m_options.find(name);

        if (iter == m_options.end())
          throw option_not_exists_exception(name);

        auto opt = iter->second;

        if (result[2].length() != 0)
          parse_option(opt, name, result[3]);

        else checked_parse_arg(argc, argv, current, opt, name);
      }
    }

    ++current;
  }

  for (auto &opt : m_options) {
    auto &detail = opt.second;
    auto &value = detail->value();

    auto &store = m_results[detail];

    if(!store.count() && value.has_default())
      parse_default(detail);
  }

  if (consume_remaining) {
    while (current < argc) {
      if (!consume_positional(argv[current]))
        break;

      ++current;
    }

    while (current != argc) {
      argv[nextKeep] = argv[current];
      ++nextKeep;
      ++current;
    }
  }

  argc = nextKeep;
}

inline void Options::add_option (
  const string &group,
  const string &s,
  const string &l,
  string desc,
  shared_ptr<const Value> value,
  string arg_help
) {
  auto stringDesc = toLocalString(std::move(desc));
  auto option = make_shared<OptionDetails>(s, l, stringDesc, value);

  if (s.size() > 0)
    add_one_option(s, option);

  if (l.size() > 0)
    add_one_option(l, option);

  auto &options = m_help[group];
  options.options.emplace_back(HelpOptionDetails{s, l, stringDesc,
      value->has_default(), value->get_default_value(),
      value->has_implicit(), value->get_implicit_value(),
      std::move(arg_help),
      value->is_container(),
      value->is_boolean()});
}

inline void Options::add_one_option (
  const string &option,
  shared_ptr<OptionDetails> details
) {
  auto in = m_options.emplace(option, details);

  if (!in.second)
    throw option_exists_error(option);
}

inline String Options::help_one_group(const string &g) const {
  typedef vector<pair<String, String>> OptionHelp;
  auto group = m_help.find(g);

  if (group == m_help.end())
    return "";

  OptionHelp format;
  size_t longest = 0;
  String result;

  if (!g.empty())
    result += toLocalString(" " + g + " options:\n");

  for (const auto &o : group->second.options) {
    if (o.is_container &&
        m_positional_set.find(o.l) != m_positional_set.end() &&
        !m_show_positional)
      continue;

    auto s = format_option(o);
    longest = max(longest, stringLength(s));
    format.push_back(make_pair(s, String()));
  }

  longest = min(longest, static_cast<size_t>(OPTION_LONGEST));
  auto allowed = size_t{76} - longest - OPTION_DESC_GAP;
  auto fiter = format.begin();

  for (const auto &o : group->second.options) {
    if (o.is_container &&
        m_positional_set.find(o.l) != m_positional_set.end() &&
        !m_show_positional)
      continue;

    auto d = format_description(o, longest + OPTION_DESC_GAP, allowed);
    result += fiter->first;

    if (stringLength(fiter->first) > longest) {
      result += '\n';
      result += toLocalString(string(longest + OPTION_DESC_GAP, ' '));
    }

    else {
      result += toLocalString(string(longest + OPTION_DESC_GAP -
        stringLength(fiter->first),
        ' '));
    }

    result += d;
    result += '\n';
    ++fiter;
  }

  return result;
}

inline void
Options::generate_group_help (
  string &result,
  const vector<string> &print_groups
) const {
  for (size_t i = 0; i != print_groups.size(); ++i) {
    const string &group_help_text = help_one_group(print_groups[i]);

    if (empty(group_help_text))
      continue;

    result += group_help_text;

    if (i < print_groups.size() - 1)
      result += '\n';
  }
}

inline void Options::generate_all_groups_help(string &result) const {
  vector<string> all_groups;
  all_groups.reserve(m_help.size());

  for (auto &group : m_help)
    all_groups.push_back(group.first);

  generate_group_help(result, all_groups);
}

inline string Options::help(const vector<string> &help_groups) const {
  String result = m_help_string + "\nUsage:\n  " +
    toLocalString(m_program) + " " + toLocalString(m_custom_help);

  if (m_positional.size() > 0 && m_positional_help.size() > 0)
    result += " " + toLocalString(m_positional_help);

  result += "\n\n";

  if (help_groups.size() == 0)
    generate_all_groups_help(result);

  else generate_group_help(result, help_groups);

  return toUTF8String(result);
}

inline const vector<string> Options::groups() const {
  vector<string> g;

  transform(
    m_help.begin(),
    m_help.end(),
    back_inserter(g),
    [] (const map<string, HelpGroupDetails>::value_type &pair) {
      return pair.first;
    });

  return g;
}

inline const HelpGroupDetails &Options::group_help(const string &group) const {
  return m_help.at(group);
}

}

#endif //CXXOPTS_HPP_INCLUDED