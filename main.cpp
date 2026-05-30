#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
#define SOCK_INVALID INVALID_SOCKET
#define sock_close closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int sock_t;
#define SOCK_INVALID (-1)
#define sock_close close
#endif

namespace {

constexpr char kAlphabet[] =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
constexpr std::size_t kBase = 62;

std::string trim(std::string s) {
  auto not_space = [](unsigned char c) { return !std::isspace(c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
  s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
  return s;
}

std::vector<std::string> split_ws(const std::string& line) {
  std::istringstream iss(line);
  std::vector<std::string> out;
  for (std::string tok; iss >> tok;) out.push_back(tok);
  return out;
}

std::string html_escape(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (std::size_t i = 0; i < s.size(); ++i) {
    switch (s[i]) {
      case '&':
        out += "&amp;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      case '"':
        out += "&quot;";
        break;
      case '\'':
        out += "&#39;";
        break;
      default:
        out.push_back(s[i]);
    }
  }
  return out;
}

std::string url_decode(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '+') {
      out.push_back(' ');
      continue;
    }
    if (s[i] == '%' && i + 2 < s.size()) {
      const char h1 = s[i + 1];
      const char h2 = s[i + 2];
      const bool ok1 = std::isxdigit(static_cast<unsigned char>(h1)) != 0;
      const bool ok2 = std::isxdigit(static_cast<unsigned char>(h2)) != 0;
      if (ok1 && ok2) {
        int v = 0;
        std::istringstream iss(s.substr(i + 1, 2));
        iss >> std::hex >> v;
        out.push_back(static_cast<char>(v));
        i += 2;
        continue;
      }
    }
    out.push_back(s[i]);
  }
  return out;
}

std::string base62_encode(uint64_t value) {
  if (value == 0) return "0";
  std::string out;
  while (value > 0) {
    out.push_back(kAlphabet[value % kBase]);
    value /= kBase;
  }
  std::reverse(out.begin(), out.end());
  return out;
}

// FNV-1a 64-bit (simple, deterministic)
uint64_t fnv1a64(const std::string& s) {
  uint64_t hash = 1469598103934665603ULL;
  for (unsigned char c : s) {
    hash ^= static_cast<uint64_t>(c);
    hash *= 1099511628211ULL;
  }
  return hash;
}

bool looks_like_url(const std::string& url) {
  // Minimal validation: must start with http:// or https://
  return url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0;
}

struct Store {
  // code -> url
  std::unordered_map<std::string, std::string> code_to_url;
  // url -> code
  std::unordered_map<std::string, std::string> url_to_code;
};

struct Shortener {
  explicit Shortener(std::string base = "http://sho.rt/")
      : base_url(std::move(base)) {
    if (!base_url.empty() && base_url.back() != '/') base_url.push_back('/');
  }

  std::string shorten(Store& store, const std::string& url) {
    {
      std::unordered_map<std::string, std::string>::const_iterator it =
          store.url_to_code.find(url);
      if (it != store.url_to_code.end()) return full_short_url(it->second);
    }

    // Deterministic seed from URL + random salt to reduce collisions across URLs
    // while preserving stability for a given run.
    uint64_t seed = fnv1a64(url) ^ random_salt();

    // Try increasing code lengths until we find an unused code.
    for (int attempt = 0; attempt < 1000; ++attempt) {
      uint64_t candidate = mix(seed + static_cast<uint64_t>(attempt));
      std::string code = base62_encode(candidate);
      // Use last N chars for nicer short codes; grow if collisions happen.
      std::size_t len = 6;
      if (attempt > 25) len = 7;
      if (attempt > 100) len = 8;
      if (code.size() > len) code = code.substr(code.size() - len);

      std::unordered_map<std::string, std::string>::const_iterator it =
          store.code_to_url.find(code);
      if (it == store.code_to_url.end()) {
        store.code_to_url.emplace(code, url);
        store.url_to_code.emplace(url, code);
        return full_short_url(code);
      }
      if (store.code_to_url[code] == url) {
        store.url_to_code.emplace(url, code);
        return full_short_url(code);
      }
    }

    throw std::runtime_error("Unable to generate a unique short code.");
  }

  bool expand(const Store& store, const std::string& code_or_url,
              std::string& out_url) const {
    std::string code = code_or_url;
    if (code.rfind(base_url, 0) == 0) code = code.substr(base_url.size());
    if (!code.empty() && code.back() == '/') code.pop_back();
    std::unordered_map<std::string, std::string>::const_iterator it =
        store.code_to_url.find(code);
    if (it != store.code_to_url.end()) {
      out_url = it->second;
      return true;
    }
    return false;
  }

  std::string full_short_url(const std::string& code) const {
    return base_url + code;
  }

  std::string base_url;

 private:
  static uint64_t mix(uint64_t x) {
    // splitmix64
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
  }

  static uint64_t random_salt() {
    static std::mt19937_64 rng{std::random_device{}()};
    static std::uniform_int_distribution<uint64_t> dist;
    return dist(rng);
  }
};

bool load_db(const std::string& path, Store& store) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;

  store.code_to_url.clear();
  store.url_to_code.clear();

  std::string line;
  while (std::getline(in, line)) {
    line = trim(line);
    if (line.empty() || line[0] == '#') continue;
    auto tab = line.find('\t');
    if (tab == std::string::npos) continue;
    std::string code = line.substr(0, tab);
    std::string url = line.substr(tab + 1);
    if (code.empty() || url.empty()) continue;
    store.code_to_url[code] = url;
    store.url_to_code[url] = code;
  }
  return true;
}

bool save_db(const std::string& path, const Store& store) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) return false;

  out << "# code<TAB>url\n";
  // Stable-ish output: dump sorted by code.
  std::vector<std::pair<std::string, std::string>> rows;
  rows.reserve(store.code_to_url.size());
  for (const auto& kv : store.code_to_url) rows.push_back(kv);
  std::sort(rows.begin(), rows.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
  for (std::size_t i = 0; i < rows.size(); ++i) {
    out << rows[i].first << '\t' << rows[i].second << "\n";
  }
  return true;
}

std::string render_home_page(const std::string& result,
                             const std::string& original_url,
                             const std::string& error) {
  std::ostringstream os;
  os << "<!doctype html><html><head><meta charset='utf-8'>"
     << "<meta name='viewport' content='width=device-width,initial-scale=1'>"
     << "<title>URL Shortener</title>"
     << "<style>"
     << ":root{color-scheme:dark;}"
     << "*{box-sizing:border-box;}"
     << "body{margin:0;font-family:Inter,Segoe UI,Arial,sans-serif;color:#e7ecff;"
     << "background:radial-gradient(circle at 10% 20%,#22316f 0%,#121a3a 42%,#0a0f22 100%);"
     << "min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px;}"
     << ".shell{width:100%;max-width:900px;background:rgba(13,18,43,.72);"
     << "border:1px solid rgba(255,255,255,.12);backdrop-filter:blur(9px);"
     << "border-radius:22px;box-shadow:0 30px 80px rgba(0,0,0,.45);overflow:hidden;}"
     << ".top{padding:24px 26px;border-bottom:1px solid rgba(255,255,255,.08);"
     << "display:flex;align-items:center;justify-content:space-between;gap:10px;}"
     << ".brand{font-size:24px;font-weight:700;letter-spacing:.3px;}"
     << ".tag{font-size:12px;color:#9fb1ff;background:rgba(122,162,255,.16);"
     << "padding:6px 10px;border-radius:999px;border:1px solid rgba(122,162,255,.3);}"
     << ".body{padding:26px;}"
     << "h1{margin:0 0 10px 0;font-size:34px;line-height:1.15;}"
     << "p{margin:0 0 20px 0;color:#b9c4ed;font-size:15px;}"
     << "form{display:flex;gap:10px;flex-wrap:wrap;}"
     << "input{flex:1 1 560px;background:#0e1535;color:#f4f7ff;border:1px solid #2d3e88;"
     << "padding:14px 15px;border-radius:12px;font-size:15px;outline:none;}"
     << "input:focus{border-color:#7da5ff;box-shadow:0 0 0 3px rgba(95,139,255,.2);}"
     << "button{background:linear-gradient(135deg,#4a7dff,#7c5dff);color:white;"
     << "border:none;padding:13px 18px;border-radius:12px;font-size:14px;font-weight:600;"
     << "cursor:pointer;transition:transform .12s ease,opacity .12s ease;}"
     << "button:hover{opacity:.95;transform:translateY(-1px);}"
     << ".result{margin-top:18px;background:linear-gradient(180deg,#142157,#0f1a47);"
     << "border:1px solid rgba(141,173,255,.35);border-radius:14px;padding:14px;}"
     << ".label{font-size:12px;color:#9db1ff;margin-bottom:8px;text-transform:uppercase;letter-spacing:.8px;}"
     << ".linkrow{display:flex;gap:10px;align-items:center;flex-wrap:wrap;}"
     << ".short-link{font-size:18px;font-weight:700;color:#dbe6ff;text-decoration:none;word-break:break-all;}"
     << ".ghost{background:#1d2c6e;border:1px solid rgba(154,182,255,.4);padding:10px 12px;}"
     << ".err{margin-top:18px;background:rgba(217,51,84,.15);border:1px solid rgba(244,111,138,.45);"
     << "padding:12px;border-radius:12px;color:#ffdbe3;}"
     << ".foot{margin-top:18px;font-size:12px;color:#8fa2e2;}"
     << "@media (max-width:620px){h1{font-size:28px;} .body{padding:20px;} .top{padding:18px;}}"
     << "</style></head><body><div class='shell'>"
     << "<div class='top'><div class='brand'>URL Shortener</div><div class='tag'>Local C++ Web App</div></div>"
     << "<div class='body'><h1>Make Long Links Look Clean</h1>"
     << "<p>Paste any URL and get a short link instantly. Reuse it anytime from your local store.</p>"
     << "<form method='POST' action='/shorten'>"
     << "<input type='text' name='url' placeholder='https://example.com/very/long/link' value='"
     << html_escape(original_url) << "' required>"
     << "<button type='submit'>Shorten URL</button></form>";

  if (!error.empty()) {
    os << "<div class='err'>" << html_escape(error) << "</div>";
  }
  if (!result.empty()) {
    os << "<div class='result'><div class='label'>Your Short URL</div>"
       << "<div class='linkrow'><a id='short-link' class='short-link' href='"
       << html_escape(result) << "'>"
       << html_escape(result) << "</a>"
       << "<button type='button' class='ghost' onclick='copyShort()'>Copy</button>"
       << "</div></div>";
  }
  os << "<div class='foot'>Tip: opening the short URL in a new tab redirects to your original link.</div>"
     << "</div></div>"
     << "<script>"
     << "function copyShort(){"
     << "var el=document.getElementById('short-link');"
     << "if(!el){return;}"
     << "var txt=el.textContent||el.innerText;"
     << "if(navigator.clipboard&&navigator.clipboard.writeText){"
     << "navigator.clipboard.writeText(txt);"
     << "}else{"
     << "var ta=document.createElement('textarea');ta.value=txt;document.body.appendChild(ta);"
     << "ta.select();document.execCommand('copy');document.body.removeChild(ta);"
     << "}"
     << "}"
     << "</script></body></html>";
  return os.str();
}

std::string get_form_value(const std::string& body, const std::string& key) {
  const std::string needle = key + "=";
  std::size_t pos = body.find(needle);
  if (pos == std::string::npos) return "";
  std::size_t start = pos + needle.size();
  std::size_t end = body.find('&', start);
  if (end == std::string::npos) end = body.size();
  return url_decode(body.substr(start, end - start));
}

void send_http_response(sock_t client, const std::string& status,
                        const std::string& content_type,
                        const std::string& body,
                        const std::string& extra_headers) {
  std::ostringstream oss;
  oss << "HTTP/1.1 " << status << "\r\n";
  oss << "Content-Type: " << content_type << "\r\n";
  oss << "Content-Length: " << body.size() << "\r\n";
  if (!extra_headers.empty()) oss << extra_headers;
  oss << "Connection: close\r\n\r\n";
  oss << body;
  const std::string resp = oss.str();
  send(client, resp.c_str(), static_cast<int>(resp.size()), 0);
}

void run_web_server(const std::string& db_path, Shortener& shortener, Store& store,
                    int port) {
#ifdef _WIN32
  WSADATA wsa_data;
  if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
    std::cerr << "Failed to initialize WinSock.\n";
    return;
  }
#endif

  sock_t server = socket(AF_INET, SOCK_STREAM, 0);
  if (server == SOCK_INVALID) {
    std::cerr << "Failed to create socket.\n";
#ifdef _WIN32
    WSACleanup();
#endif
    return;
  }

  int reuse = 1;
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR,
             reinterpret_cast<const char*>(&reuse), sizeof(reuse));

  sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(static_cast<uint16_t>(port));

  if (bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    std::cerr << "Port " << port << " is busy or unavailable.\n";
    sock_close(server);
#ifdef _WIN32
    WSACleanup();
#endif
    return;
  }
  if (listen(server, 128) != 0) {
    std::cerr << "listen() failed.\n";
    sock_close(server);
#ifdef _WIN32
    WSACleanup();
#endif
    return;
  }

  std::cout << "Web UI running on port " << port << "\n";
  std::cout << "Press Ctrl+C to stop.\n";

  for (;;) {
    sock_t client = accept(server, NULL, NULL);
    if (client == SOCK_INVALID) continue;

    std::string req;
    char buffer[4096];
    int content_length = 0;
    bool headers_parsed = false;
    std::size_t headers_end_pos = std::string::npos;
    for (;;) {
      const int received = recv(client, buffer, sizeof(buffer), 0);
      if (received <= 0) break;
      req.append(buffer, buffer + received);

      if (!headers_parsed) {
        headers_end_pos = req.find("\r\n\r\n");
        if (headers_end_pos != std::string::npos) {
          headers_parsed = true;
          const std::string headers = req.substr(0, headers_end_pos);
          const std::string needle = "Content-Length:";
          std::size_t pos = headers.find(needle);
          if (pos != std::string::npos) {
            std::size_t value_start = pos + needle.size();
            while (value_start < headers.size() &&
                   (headers[value_start] == ' ' ||
                    headers[value_start] == '\t')) {
              ++value_start;
            }
            std::size_t value_end = headers.find("\r\n", value_start);
            if (value_end == std::string::npos) value_end = headers.size();
            content_length =
                std::atoi(headers.substr(value_start, value_end - value_start)
                              .c_str());
            if (content_length < 0) content_length = 0;
          }
        }
      }

      if (headers_parsed) {
        const std::size_t body_start = headers_end_pos + 4;
        const std::size_t have_body =
            req.size() > body_start ? req.size() - body_start : 0;
        if (have_body >= static_cast<std::size_t>(content_length)) break;
      }
    }
    if (req.empty()) {
      sock_close(client);
      continue;
    }

    const std::size_t line_end = req.find("\r\n");
    if (line_end == std::string::npos) {
      sock_close(client);
      continue;
    }
    const std::string request_line = req.substr(0, line_end);
    std::vector<std::string> parts = split_ws(request_line);
    if (parts.size() < 2) {
      sock_close(client);
      continue;
    }
    const std::string method = parts[0];
    const std::string path = parts[1];

    if (method == "GET" && path == "/") {
      send_http_response(client, "200 OK", "text/html; charset=utf-8",
                         render_home_page("", "", ""), "");
    } else if (method == "POST" && path == "/shorten") {
      std::string body;
      const std::size_t body_pos = req.find("\r\n\r\n");
      if (body_pos != std::string::npos) body = req.substr(body_pos + 4);
      const std::string submitted = trim(get_form_value(body, "url"));

      if (!looks_like_url(submitted)) {
        send_http_response(
            client, "400 Bad Request", "text/html; charset=utf-8",
            render_home_page("", submitted,
                             "URL must start with http:// or https://"),
            "");
      } else {
        try {
          const std::string short_url = shortener.shorten(store, submitted);
          save_db(db_path, store);
          send_http_response(client, "200 OK", "text/html; charset=utf-8",
                             render_home_page(short_url, submitted, ""), "");
        } catch (const std::exception& e) {
          send_http_response(client, "500 Internal Server Error",
                             "text/html; charset=utf-8",
                             render_home_page("", submitted, e.what()), "");
        }
      }
    } else if (method == "GET" && path.size() > 1) {
      std::string maybe_code = path.substr(1);
      std::string url;
      if (shortener.expand(store, maybe_code, url)) {
        send_http_response(client, "302 Found", "text/plain",
                           "Redirecting...",
                           "Location: " + url + "\r\n");
      } else {
        send_http_response(client, "404 Not Found", "text/plain",
                           "Short code not found.", "");
      }
    } else {
      send_http_response(client, "404 Not Found", "text/plain", "Not found.",
                         "");
    }

    sock_close(client);
  }
}

void run_interactive(const std::string& db_path, Shortener& shortener,
                     Store& store) {
  std::cout << "URL Shortener Interactive Mode\n";
  std::cout << "Type a URL (http:// or https://) to shorten it instantly.\n";
  std::cout << "Commands: expand <code|short-url>, list, help, exit\n\n";

  std::string line;
  while (true) {
    std::cout << "> ";
    if (!std::getline(std::cin, line)) {
      std::cout << "\n";
      break;
    }
    line = trim(line);
    if (line.empty()) continue;

    if (line == "exit" || line == "quit") break;

    if (looks_like_url(line)) {
      try {
        std::string short_url = shortener.shorten(store, line);
        if (!save_db(db_path, store)) {
          std::cerr << "Warning: failed to save " << db_path << "\n";
        }
        std::cout << short_url << "\n";
      } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
      }
      continue;
    }

    std::vector<std::string> parts = split_ws(line);
    if (parts.empty()) continue;

    if (parts[0] == "help") {
      std::cout << "Usage:\n";
      std::cout << "  <url>                 Shorten URL\n";
      std::cout << "  expand <code|short>   Expand short code/URL\n";
      std::cout << "  list                  Show all mappings\n";
      std::cout << "  exit                  Quit\n";
      continue;
    }

    if (parts[0] == "list") {
      std::vector<std::pair<std::string, std::string>> rows;
      rows.reserve(store.code_to_url.size());
      for (const auto& kv : store.code_to_url) rows.push_back(kv);
      std::sort(rows.begin(), rows.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });
      for (std::size_t i = 0; i < rows.size(); ++i) {
        std::cout << std::left << std::setw(10) << rows[i].first << "  "
                  << rows[i].second << "\n";
      }
      continue;
    }

    if (parts[0] == "expand") {
      if (parts.size() < 2) {
        std::cerr << "Usage: expand <code|short-url>\n";
        continue;
      }
      std::string url;
      if (!shortener.expand(store, parts[1], url)) {
        std::cerr << "Not found.\n";
      } else {
        std::cout << url << "\n";
      }
      continue;
    }

    std::cerr << "Unknown input. Type 'help' for commands.\n";
  }
}

void print_help(const std::string& exe) {
  std::cout
      << "URL Shortener (local)\n\n"
      << "Usage:\n"
      << "  " << exe << " web [port]\n"
      << "  " << exe << " interactive\n"
      << "  " << exe << " shorten <https://...>\n"
      << "  " << exe << " expand <code|short-url>\n"
      << "  " << exe << " list\n"
      << "  " << exe << " help\n\n"
      << "Notes:\n"
      << "  - Stores mappings in urls.db in the current folder.\n"
      << "  - Base URL for output can be set with SHORT_BASE, e.g.\n"
      << "      set SHORT_BASE=https://my.dom/\n";
}

}  // namespace

int main(int argc, char** argv) {
  const std::string db_path = "urls.db";
  const char* env_base = std::getenv("SHORT_BASE");
  Shortener shortener(env_base ? std::string(env_base) : "http://sho.rt/");

  Store store;
  load_db(db_path, store);  // ok if it doesn't exist

  const std::string exe = (argc > 0 && argv[0]) ? argv[0] : "url_shortener";
  if (argc < 2) {
    run_interactive(db_path, shortener, store);
    return 0;
  }

  const std::string cmd = argv[1];
  if (cmd == "help" || cmd == "--help" || cmd == "-h") {
    print_help(exe);
    return 0;
  }
  if (cmd == "web") {
    int port = 8080;
    const char* env_port = std::getenv("PORT");
    if (env_port && env_port[0]) {
      port = std::atoi(env_port);
    } else if (argc >= 3) {
      port = std::atoi(argv[2]);
    }
    if (port <= 0 || port > 65535) {
      std::cerr << "Invalid port.\n";
      return 2;
    }

    const char* public_url = std::getenv("SHORT_BASE");
    if (!public_url || !public_url[0]) public_url = std::getenv("RENDER_EXTERNAL_URL");
    if (!public_url || !public_url[0]) public_url = std::getenv("PUBLIC_URL");

    if (public_url && public_url[0]) {
      shortener.base_url = public_url;
      if (!shortener.base_url.empty() && shortener.base_url.back() != '/') {
        shortener.base_url.push_back('/');
      }
    } else {
      std::ostringstream base;
      base << "http://localhost:" << port << "/";
      shortener.base_url = base.str();
    }

    run_web_server(db_path, shortener, store, port);
    return 0;
  }
  if (cmd == "interactive" || cmd == "i") {
    run_interactive(db_path, shortener, store);
    return 0;
  }

  if (cmd == "shorten") {
    if (argc < 3) {
      std::cerr << "Missing URL.\n";
      return 2;
    }
    std::string url = argv[2];
    if (!looks_like_url(url)) {
      std::cerr << "URL must start with http:// or https://\n";
      return 2;
    }

    try {
      std::string short_url = shortener.shorten(store, url);
      if (!save_db(db_path, store)) {
        std::cerr << "Warning: failed to save " << db_path << "\n";
      }
      std::cout << short_url << "\n";
      return 0;
    } catch (const std::exception& e) {
      std::cerr << "Error: " << e.what() << "\n";
      return 3;
    }
  }

  if (cmd == "expand") {
    if (argc < 3) {
      std::cerr << "Missing code or short URL.\n";
      return 2;
    }
    std::string input = argv[2];
    std::string url;
    if (!shortener.expand(store, input, url)) {
      std::cerr << "Not found.\n";
      return 4;
    }
    std::cout << url << "\n";
    return 0;
  }

  if (cmd == "list") {
    std::vector<std::pair<std::string, std::string>> rows;
    rows.reserve(store.code_to_url.size());
    for (const auto& kv : store.code_to_url) rows.push_back(kv);
    std::sort(rows.begin(), rows.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    for (std::size_t i = 0; i < rows.size(); ++i) {
      const std::string& code = rows[i].first;
      const std::string& url = rows[i].second;
      std::cout << std::left << std::setw(10) << code << "  " << url << "\n";
    }
    return 0;
  }

  std::cerr << "Unknown command: " << cmd << "\n";
  print_help(exe);
  return 2;
}

