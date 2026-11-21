#include "json.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static const Json kNull;

const Json& Json::operator[](size_t i) const { return type_ == Array && i < arr_.size() ? arr_[i] : kNull; }

const Json& Json::operator[](const std::string& k) const {
  if (type_ != Object) return kNull;
  auto it = obj_.find(k);
  return it == obj_.end() ? kNull : order_[it->second].second;
}

void Json::set(const std::string& k, const Json& v) {
  type_ = Object;
  auto it = obj_.find(k);
  if (it != obj_.end()) order_[it->second].second = v;
  else {
    obj_[k] = order_.size();
    order_.emplace_back(k, v);
  }
}

struct JsonParser {
  const char* p;
  const char* end;
  bool ok = true;

  void ws() {
    while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
  }
  static void utf8(std::string& o, unsigned cp) {
    if (cp < 0x80) o += (char)cp;
    else if (cp < 0x800) { o += (char)(0xC0 | (cp >> 6)); o += (char)(0x80 | (cp & 63)); }
    else if (cp < 0x10000) { o += (char)(0xE0 | (cp >> 12)); o += (char)(0x80 | ((cp >> 6) & 63)); o += (char)(0x80 | (cp & 63)); }
    else { o += (char)(0xF0 | (cp >> 18)); o += (char)(0x80 | ((cp >> 12) & 63)); o += (char)(0x80 | ((cp >> 6) & 63)); o += (char)(0x80 | (cp & 63)); }
  }
  std::string string() {
    std::string o;
    p++;  // "
    while (p < end && *p != '"') {
      if (*p == '\\' && p + 1 < end) {
        p++;
        char c = *p++;
        switch (c) {
          case 'n': o += '\n'; break;
          case 't': o += '\t'; break;
          case 'r': o += '\r'; break;
          case 'b': o += '\b'; break;
          case 'f': o += '\f'; break;
          case 'u': {
            unsigned cp = (unsigned)strtoul(std::string(p, p + 4).c_str(), nullptr, 16);
            p += 4;
            if (cp >= 0xD800 && cp < 0xDC00 && p + 6 <= end && p[0] == '\\' && p[1] == 'u') {
              unsigned lo = (unsigned)strtoul(std::string(p + 2, p + 6).c_str(), nullptr, 16);
              p += 6;
              cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
            }
            utf8(o, cp);
            break;
          }
          default: o += c;
        }
      } else {
        o += *p++;
      }
    }
    p++;
    return o;
  }
  Json value() {
    ws();
    if (p >= end) { ok = false; return Json(); }
    char c = *p;
    if (c == '{') {
      Json j = Json::object();
      p++;
      ws();
      if (*p == '}') { p++; return j; }
      while (p < end) {
        ws();
        if (*p != '"') { ok = false; return j; }
        std::string k = string();
        ws();
        if (*p == ':') p++;
        j.set(k, value());
        ws();
        if (*p == ',') { p++; continue; }
        if (*p == '}') { p++; break; }
        ok = false;
        break;
      }
      return j;
    }
    if (c == '[') {
      Json j = Json::array();
      p++;
      ws();
      if (*p == ']') { p++; return j; }
      while (p < end) {
        j.push(value());
        ws();
        if (*p == ',') { p++; continue; }
        if (*p == ']') { p++; break; }
        ok = false;
        break;
      }
      return j;
    }
    if (c == '"') return Json(string());
    if (!strncmp(p, "true", 4)) { p += 4; return Json(true); }
    if (!strncmp(p, "false", 5)) { p += 5; return Json(false); }
    if (!strncmp(p, "null", 4)) { p += 4; return Json(); }
    char* e;
    double d = strtod(p, &e);
    if (e == p) { ok = false; p++; return Json(); }
    p = e;
    return Json(d);
  }
};

Json Json::parse(const std::string& text, bool* ok) {
  // As traduções do Google começam com ")]}'" (proteção anti-XSSI): ignora até ao primeiro '{' ou '['.
  size_t start = text.find_first_of("{[");
  if (start == std::string::npos) {
    if (ok) *ok = false;
    return Json();
  }
  JsonParser jp{text.data() + start, text.data() + text.size()};
  Json j = jp.value();
  if (ok) *ok = jp.ok;
  return j;
}

static void esc(std::string& o, const std::string& s) {
  o += '"';
  for (char c : s) {
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if (c == '\n') o += "\\n";
    else o += c;
  }
  o += '"';
}

std::string Json::dump() const {
  std::string o;
  char buf[64];
  switch (type_) {
    case Null: return "null";
    case Bool: return num_ ? "true" : "false";
    case Number:
      snprintf(buf, sizeof buf, "%.10g", num_);
      return buf;
    case String: esc(o, str_); return o;
    case Array:
      o = "[";
      for (size_t i = 0; i < arr_.size(); i++) { if (i) o += ","; o += arr_[i].dump(); }
      return o + "]";
    case Object:
      o = "{";
      for (size_t i = 0; i < order_.size(); i++) {
        if (i) o += ",";
        esc(o, order_[i].first);
        o += ":" + order_[i].second.dump();
      }
      return o + "}";
  }
  return o;
}
