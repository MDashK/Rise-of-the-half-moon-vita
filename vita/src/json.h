#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

// Parser/serializador JSON simples (suficiente para os dados do jogo e as definições).
class Json {
 public:
  enum Type { Null, Bool, Number, String, Array, Object };

  Json() = default;
  explicit Json(bool b) : type_(Bool), num_(b ? 1 : 0) {}
  explicit Json(double n) : type_(Number), num_(n) {}
  explicit Json(const std::string& s) : type_(String), str_(s) {}
  static Json array() { Json j; j.type_ = Array; return j; }
  static Json object() { Json j; j.type_ = Object; return j; }

  static Json parse(const std::string& text, bool* ok = nullptr);
  std::string dump() const;

  Type type() const { return type_; }
  bool isNull() const { return type_ == Null; }
  bool isArray() const { return type_ == Array; }
  bool isObject() const { return type_ == Object; }
  bool isString() const { return type_ == String; }
  bool isNumber() const { return type_ == Number; }

  double num(double def = 0) const { return type_ == Number || type_ == Bool ? num_ : def; }
  int integer(int def = 0) const { return type_ == Number || type_ == Bool ? (int)num_ : def; }
  bool boolean(bool def = false) const { return type_ == Bool || type_ == Number ? num_ != 0 : def; }
  const std::string& str() const { return str_; }
  std::string str(const std::string& def) const { return type_ == String ? str_ : def; }

  size_t size() const { return type_ == Array ? arr_.size() : type_ == Object ? obj_.size() : 0; }
  const Json& operator[](size_t i) const;
  const Json& operator[](const std::string& k) const;
  bool has(const std::string& k) const { return type_ == Object && obj_.count(k) > 0; }
  const std::vector<Json>& items() const { return arr_; }
  const std::vector<std::pair<std::string, Json>>& members() const { return order_; }

  void push(const Json& v) { type_ = Array; arr_.push_back(v); }
  void set(const std::string& k, const Json& v);

 private:
  Type type_ = Null;
  double num_ = 0;
  std::string str_;
  std::vector<Json> arr_;
  std::map<std::string, size_t> obj_;
  std::vector<std::pair<std::string, Json>> order_;
  friend struct JsonParser;
};
