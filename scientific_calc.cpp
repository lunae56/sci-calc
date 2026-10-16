// ============================================================================
//  scientific_calc.cpp —— 控制台科学计算器
//
//  纯 C++ 标准库实现，零外部依赖，不使用 vector / map / string 流等重型设施。
//  解析方式：递归下降（recursive descent），不借助任何第三方表达式库。
//
//  --- 支持的内容 -------------------------------------------------------------
//  运算符     +  -  *  /  %(取模)  ^(乘方)  ( )
//  一元       -x（取负）   +x（保留符号）
//  函数       sqrt sin cos tan asin acos atan ln log(以10为底)
//             abs exp floor ceil round
//  简写       √x  等价于 sqrt(x)          （UTF-8 三字节，代码里手写解析）
//  常量       pi / PI / π      e
//  数字       123   3.14   .5   1e5   2.5E-3
//  角度单位   角度制（默认）/ 弧度制，用 --deg / --rad 或交互里输入 deg/rad 切换
//  额外       plot f(x) a b [n] —— 在终端用字符画函数图像，不需要图形库（n 默认 70）
//
//  --- 错误处理 ---------------------------------------------------------------
//  除零、非法字符、括号不匹配、缺右操作数、函数参数个数不对、
//  对负数开平方、乘方溢出、函数定义域错误、数字格式错误……统一报错并指出位置
//
//  --- 编译（任选其一）--------------------------------------------------------
//    cl      /std:c++17 /utf-8 /W4 /O2 /EHsc scientific_calc.cpp   (MSVC)
//    g++     -std=c++17 -O2 -Wall -Wextra -o sci_calc scientific_calc.cpp
//    clang++ -std=c++17 -O2 -Wall -Wextra -o sci_calc scientific_calc.cpp
//
//  --- 运行 -------------------------------------------------------------------
//    ./sci_calc                      交互模式，角度制
//    ./sci_calc "2+3*4"              直接算一个表达式
//    ./sci_calc --rad "sin(pi/2)"    弧度模式算一次
// ============================================================================

#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <string>
#include <iostream>
#include <fstream>

// Windows 上 stdin/stdout 默认是文本模式，会把 CRLF 转成 LF、把 0x1A 当 EOF。
// 批处理要按原样读字节，所以需要 _setmode + _O_BINARY（仅 MSVC/Windows 需要）。
#if defined(_WIN32)
#  include <io.h>
#  include <fcntl.h>
#endif

// MSVC 会把 fopen 这类标准函数报成「不安全」并建议换成它专有的 _s 版本。
// 这里用的是可移植的标准写法，所以显式关掉这条警告（其它编译器看不到这个宏）。
#if defined(_MSC_VER)
#  pragma warning(disable : 4996)
#endif

// Windows 下需要这两个 API 做编码转换（见下面的输出层）
#if defined(_WIN32)
#  include <windows.h>
#endif

namespace {

const double kPi = 3.14159265358979323846;
const double kE  = 2.71828182845904523536;

// ===========================================================================
// 输出层：解决中文乱码
// ---------------------------------------------------------------------------
// 问题：源码里的中文在可执行文件里是 UTF-8 字节，直接写到 stdout 就是 UTF-8。
//       而 Windows 中文系统的控制台默认代码页是 936(GBK)，它按 GBK 去解读这些
//       UTF-8 字节，于是「错误」显示成「閿欒」。
// 解决：写到「控制台」时，把 UTF-8 转成当前控制台的输出代码页再写；
//       写到「文件/管道」时保持原样 UTF-8（这样重定向出来的文件仍是标准 UTF-8，
//       而且 cmd 重定向时控制台代码页不会跟着变，转成 GBK 反而会污染文件）。
// ===========================================================================
#if defined(_WIN32)

bool stdoutIsConsole() {
    static int cached = -1;
    if (cached < 0) {
        DWORD mode = 0;
        cached = GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode) ? 1 : 0;
    }
    return cached == 1;
}

std::string utf8ToConsoleEncoding(const std::string& utf8) {
    UINT cp = GetConsoleOutputCP();
    if (cp == 0 || cp == CP_UTF8) return utf8;      // 已经是 UTF-8，不用转

    // 第一步：UTF-8 -> UTF-16
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), 0, 0);
    if (wideLen <= 0) return utf8;
    std::string wideBuf((std::size_t)wideLen * sizeof(wchar_t), '\0');
    wchar_t* wide = reinterpret_cast<wchar_t*>(&wideBuf[0]);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), wide, wideLen);

    // 第二步：UTF-16 -> 控制台代码页
    int narrowLen = WideCharToMultiByte(cp, 0, wide, wideLen, 0, 0, 0, 0);
    if (narrowLen <= 0) return utf8;
    std::string out((std::size_t)narrowLen, '\0');
    WideCharToMultiByte(cp, 0, wide, wideLen, &out[0], narrowLen, 0, 0);
    return out;
}

#endif  // _WIN32

// 所有文本输出都走这里
void outText(const std::string& s) {
#if defined(_WIN32)
    if (stdoutIsConsole()) {
        std::string converted = utf8ToConsoleEncoding(s);
        std::fwrite(converted.data(), 1, converted.size(), stdout);
        return;
    }
#endif
    std::fwrite(s.data(), 1, s.size(), stdout);
}

void outLine(const std::string& s) {
    outText(s);
    outText("\n");
}

// printf 风格，内部先格式化再走编码转换
void outFormat(const char* format, ...) {
    char buf[8192];
    va_list ap;
    va_start(ap, format);
    std::vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    outText(std::string(buf));
}

// ---------------------------------------------------------------------------
// 数值判定：直接用 <cmath> 的标准设施
// ---------------------------------------------------------------------------
inline bool isFiniteD(double v) { return std::isfinite(v); }

// ---------------------------------------------------------------------------
// 错误类型：统一承载「错误信息 + 出错位置」
// ---------------------------------------------------------------------------
struct CalcError {
    std::string msg;
    std::size_t pos;                 // 出错位置（从 0 起），用于画 ^ 指示
    CalcError(const std::string& m, std::size_t p) : msg(m), pos(p) {}
};

// ---------------------------------------------------------------------------
// 角度单位
// ---------------------------------------------------------------------------
enum AngleMode { kDegree, kRadian };

AngleMode g_angleMode = kDegree;     // 默认角度制，符合学生计算器习惯

inline double toRadians(double v) {
    return (g_angleMode == kDegree) ? v * kPi / 180.0 : v;
}
inline double fromRadians(double v) {
    return (g_angleMode == kDegree) ? v * 180.0 / kPi : v;
}

// ---------------------------------------------------------------------------
// 字符判断：一律转 unsigned char，避免中文等高位字节触发断言
// ---------------------------------------------------------------------------
inline bool isDigitCh(char c) { return c >= '0' && c <= '9'; }
inline bool isAlphaCh(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
inline bool isIdentCh(char c) {
    return isAlphaCh(c) || c == '_' || (unsigned char)c >= 0x80;
}

// 一个 UTF-8 字符占几个字节（按首字节判断，非法首字节按 1 处理）
inline std::size_t utf8Len(unsigned char lead) {
    if ((lead & 0x80) == 0x00) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

// 取出 s[pos] 开始的完整一个 UTF-8 字符。
// 报错信息里如果只截一个字节，多字节字符（比如 π）会变成乱码，
// 所以这里按首字节的长度整体取出来。
std::string utf8CharAt(const std::string& s, std::size_t pos) {
    if (pos >= s.size()) return std::string();
    std::size_t len = utf8Len((unsigned char)s[pos]);
    if (pos + len > s.size()) len = 1;
    return s.substr(pos, len);
}

// 不区分大小写比较（只处理 ASCII 字母，无需 locale）
bool equalsIgnoreCase(const std::string& a, const char* b) {
    std::size_t i = 0;
    for (; i < a.size(); ++i) {
        if (b[i] == '\0') return false;
        char x = a[i];
        char y = b[i];
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y) return false;
    }
    return b[i] == '\0';
}

// ---------------------------------------------------------------------------
// 函数表：固定数组，不依赖 map
// ---------------------------------------------------------------------------
typedef double (*MathFn)(double);

struct FnEntry {
    const char* name;
    MathFn fn;
};

double fnLog10(double v) { return std::log10(v); }

const FnEntry kFunctions[] = {
    { "sqrt",  std::sqrt  },
    { "sin",   std::sin   },
    { "cos",   std::cos   },
    { "tan",   std::tan   },
    { "asin",  std::asin  },
    { "acos",  std::acos  },
    { "atan",  std::atan  },
    { "ln",    std::log   },
    { "log",   fnLog10    },
    { "abs",   std::fabs  },
    { "exp",   std::exp   },
    { "floor", std::floor },
    { "ceil",  std::ceil  },
    { "round", std::round },
};

const std::size_t kFunctionCount = sizeof(kFunctions) / sizeof(kFunctions[0]);
const std::size_t kNoFunction = (std::size_t)-1;

std::size_t findFunction(const std::string& name) {
    for (std::size_t i = 0; i < kFunctionCount; ++i) {
        if (equalsIgnoreCase(name, kFunctions[i].name)) return i;
    }
    return kNoFunction;
}

// ---------------------------------------------------------------------------
// 结果类型：把「成功 / 失败」当返回值带出来，不用异常穿透整个程序
// ---------------------------------------------------------------------------
struct ParseResult {
    bool ok;
    double value;
    CalcError error;
    ParseResult() : ok(false), value(0.0), error("", 0) {}
    explicit ParseResult(double v) : ok(true), value(v), error("", 0) {}
    explicit ParseResult(const CalcError& e) : ok(false), value(0.0), error(e) {}
};

std::string formatNumber(double v, int sig) {
    char buf[64];
    char pattern[16];
    std::snprintf(pattern, sizeof(pattern), "%%.%dg", sig);
    std::snprintf(buf, sizeof(buf), pattern, v);
    return std::string(buf);
}

// ===========================================================================
// 递归下降解析器
// ---------------------------------------------------------------------------
// 文法（优先级由低到高）：
//   expr    := term  { ("+" | "-") term }
//   term    := unary { ("*" | "/" | "%") unary }
//   unary   := ("+" | "-") unary | power
//   power   := primary [ "^" unary ]        // 右侧递归 → 右结合，且允许 2^-3
//   primary := number | "(" expr ")" | name "(" expr ")" | "√" unary | name
// ===========================================================================
class Parser {
public:
    Parser(const std::string& src, double xValue)
        : s_(src), n_(src.size()), pos_(0), x_(xValue), usesX_(false), depth_(0) {}

    bool usesX() const { return usesX_; }

    double run() {
        double v = parseExpr();
        skipSpace();
        if (pos_ < n_) {
            fail("多余的内容 '" + utf8CharAt(s_, pos_) + "'");
        }
        return v;
    }

private:
    const std::string& s_;
    std::size_t n_;
    std::size_t pos_;
    double x_;
    bool usesX_;

    // 嵌套深度。括号、函数调用、√ 都会让解析器递归，而递归下降每层要
    // 经过 expr→term→unary→power→primary 五个函数。不加限制的话，几
    // 千层括号就能把默认 1MB 的线程栈吃穿，导致栈溢出崩溃（0xC00000FD）。
    int depth_;
    static const int kMaxDepth = 200;

    // RAII 守卫：进入一层递归就 +1，离开时（含抛异常）自动 -1，
    // 不用在每个 return 分支手动维护。
    class DepthGuard {
    public:
        explicit DepthGuard(Parser& p) : p_(p) {
            if (++p_.depth_ > kMaxDepth) {
                --p_.depth_;                          // 先把计数还回去，避免重复报错时越界
                p_.fail("表达式嵌套太深，最多 " + std::to_string(kMaxDepth) + " 层");
            }
        }
        ~DepthGuard() { --p_.depth_; }
    private:
        Parser& p_;
        DepthGuard(const DepthGuard&);
        DepthGuard& operator=(const DepthGuard&);
    };

    // 解析器持有输入串的引用，不允许拷贝；显式声明可避免老编译器的警告
    Parser(const Parser&);
    Parser& operator=(const Parser&);

    // ---- 基础工具 ----------------------------------------------------------
    void skipSpace() {
        while (pos_ < n_ && (s_[pos_] == ' ' || s_[pos_] == '\t' ||
                             s_[pos_] == '\r' || s_[pos_] == '\n')) {
            ++pos_;
        }
    }

    char peek() {
        skipSpace();
        return (pos_ < n_) ? s_[pos_] : '\0';
    }

    char at(std::size_t i) const { return (i < n_) ? s_[i] : '\0'; }

    void fail(const std::string& msg) const { throw CalcError(msg, pos_); }

    bool match(char c) {
        skipSpace();
        if (pos_ < n_ && s_[pos_] == c) { ++pos_; return true; }
        return false;
    }

    // 读取一个标识符（不移动 pos_）
    std::string peekWord() {
        skipSpace();
        if (pos_ >= n_ || !isIdentCh(s_[pos_])) return std::string();
        std::size_t p = pos_;
        while (p < n_ && (isIdentCh(s_[p]) || isDigitCh(s_[p]))) ++p;
        return s_.substr(pos_, p - pos_);
    }

    // ---- 文法各级 ----------------------------------------------------------
    double parseExpr() {
        double v = parseTerm();
        for (;;) {
            char c = peek();
            if (c == '+')      { ++pos_; v += parseTerm(); }
            else if (c == '-') { ++pos_; v -= parseTerm(); }
            else               return v;
        }
    }

    double parseTerm() {
        double v = parseUnary();
        for (;;) {
            char c = peek();
            if (c == '*') {
                ++pos_;
                v *= parseUnary();
            } else if (c == '/') {
                std::size_t opPos = pos_;
                ++pos_;
                double d = parseUnary();
                if (d == 0.0) throw CalcError("除以零", opPos);
                v /= d;
            } else if (c == '%') {
                std::size_t opPos = pos_;
                ++pos_;
                double d = parseUnary();
                if (d == 0.0) throw CalcError("对零取模", opPos);
                v = std::fmod(v, d);
            } else {
                return v;
            }
        }
    }

    double parseUnary() {
        char c = peek();
        if (c == '-') { ++pos_; return -parseUnary(); }
        if (c == '+') { ++pos_; return  parseUnary(); }
        return parsePower();
    }

    double parsePower() {
        double base = parsePrimary();
        skipSpace();
        if (pos_ < n_ && s_[pos_] == '^') {
            std::size_t opPos = pos_;
            ++pos_;
            double e = parseUnary();              // 2^3^2 == 2^(3^2)
            return applyPow(base, e, opPos);
        }
        return base;
    }

    double applyPow(double base, double e, std::size_t opPos) const {
        if (base < 0.0 && std::floor(e) != e) {
            throw CalcError("负数底数配非整数指数，结果不是实数", opPos);
        }
        if (base == 0.0 && e < 0.0) {
            throw CalcError("0 的负数次幂（相当于除以零）", opPos);
        }
        double r = std::pow(base, e);
        if (std::isnan(r)) throw CalcError("乘方结果无定义", opPos);
        if (std::isinf(r)) throw CalcError("乘方结果溢出", opPos);
        return r;
    }

    double parsePrimary() {
        skipSpace();
        if (pos_ >= n_) fail("表达式意外结束，缺少操作数");

        char c = s_[pos_];

        // --- 数字 ---
        if (isDigitCh(c) || (c == '.' && isDigitCh(at(pos_ + 1)))) {
            return parseNumber();
        }

        // --- 括号 ---
        if (c == '(') {
            ++pos_;
            DepthGuard guard(*this);
            double v = parseExpr();
            skipSpace();
            if (pos_ >= n_ || s_[pos_] != ')') fail("缺少右括号 ')'");
            ++pos_;
            return v;
        }
        if (c == ')') fail("多余的右括号 ')'");

        // --- 一元开方简写 √（UTF-8: E2 88 9A）---
        // 与 sin(30) 那类函数不同，√ 直接接一个一元表达式：√9、√-4、√2^2
        if ((unsigned char)c == 0xE2 && (unsigned char)at(pos_ + 1) == 0x88 &&
            (unsigned char)at(pos_ + 2) == 0x9A) {
            std::size_t opPos = pos_;
            pos_ += 3;
            DepthGuard guard(*this);
            double v = parseUnary();
            if (v < 0.0) throw CalcError("对负数开平方", opPos);
            return std::sqrt(v);
        }

        // --- 标识符：函数名 / 常量 / 变量 ---
        if (isIdentCh(c)) {
            std::size_t wordStart = pos_;
            std::string word = peekWord();

            // 1) 已知函数
            if (findFunction(word) != kNoFunction) {
                pos_ += word.size();
                skipSpace();
                if (pos_ >= n_ || s_[pos_] != '(') {
                    pos_ = wordStart;
                    fail("函数 " + word + " 后面需要括号，例如 " + word + "(2)");
                }
                ++pos_;                                   // 吃掉 '('

                // 空参数要单独报错，否则会误导成「多余的右括号」
                skipSpace();
                if (pos_ < n_ && s_[pos_] == ')') {
                    fail("函数 " + word + " 缺少参数，例如 " + word + "(2)");
                }

                DepthGuard guard(*this);
                double arg = parseExpr();
                skipSpace();
                if (pos_ >= n_ || s_[pos_] != ')') fail("函数 " + word + " 缺少右括号 ')'");
                ++pos_;                                   // 吃掉 ')'
                return applyFunction(word, arg);
            }

            // 2) 常量
            if (equalsIgnoreCase(word, "pi") || word == "\xCF\x80") {   // π
                pos_ += word.size();
                return kPi;
            }
            if (equalsIgnoreCase(word, "e")) {
                pos_ += word.size();
                return kE;
            }
            // 3) 变量 x（供 plot 使用）
            if (equalsIgnoreCase(word, "x")) {
                pos_ += word.size();
                usesX_ = true;
                return x_;
            }

            fail("未知的名称 '" + word + "'");
        }

        fail("非法字符 '" + utf8CharAt(s_, pos_) + "'");
        return 0.0;   // 不可达，仅为消除「缺少返回值」警告
    }

    // 数字：123 / 3.14 / .5 / 1e5 / 2.5E-3
    double parseNumber() {
        std::size_t start = pos_;
        while (pos_ < n_ && isDigitCh(s_[pos_])) ++pos_;
        if (pos_ < n_ && s_[pos_] == '.') {
            ++pos_;
            while (pos_ < n_ && isDigitCh(s_[pos_])) ++pos_;
        }
        if (pos_ < n_ && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
            std::size_t save = pos_;
            ++pos_;
            if (pos_ < n_ && (s_[pos_] == '+' || s_[pos_] == '-')) ++pos_;
            if (pos_ < n_ && isDigitCh(s_[pos_])) {
                while (pos_ < n_ && isDigitCh(s_[pos_])) ++pos_;
            } else {
                pos_ = save;          // 不是科学计数法，退回（e 当常量处理）
            }
        }
        std::string text = s_.substr(start, pos_ - start);

        // 数字后面紧贴字母是常见笔误（1e、2pi），必须报错而不是静默按隐式乘法算
        if (pos_ < n_ && isIdentCh(s_[pos_])) {
            std::string followed = utf8CharAt(s_, pos_);
            fail("数字 " + text + " 后面不能直接接 '" + followed + "'，请加运算符（例如 " + text + "*" + followed + "）");
        }

        const char* begin = text.c_str();
        char* end = 0;
        double v = std::strtod(begin, &end);
        if (end != begin + text.size()) fail("数字格式不正确: " + text);
        if (std::isinf(v)) fail("数字超出可表示范围: " + text);
        return v;
    }

    // 调用函数，并做定义域检查
    double applyFunction(const std::string& word, double a) const {
        std::size_t fi = findFunction(word);
        if (fi == kNoFunction) throw CalcError("未知函数 '" + word + "'", pos_);

        double arg = a;
        if (equalsIgnoreCase(word, "sin") || equalsIgnoreCase(word, "cos") ||
            equalsIgnoreCase(word, "tan")) {
            arg = toRadians(a);
        }

        double r = kFunctions[fi].fn(arg);

        if (std::isnan(r)) {
            throw CalcError("函数 " + word + " 在 x=" + formatNumber(a, 12) +
                            " 处无定义（定义域错误）", pos_);
        }
        if (std::isinf(r)) {
            throw CalcError("函数 " + word + " 结果溢出", pos_);
        }
        if (equalsIgnoreCase(word, "asin") || equalsIgnoreCase(word, "acos") ||
            equalsIgnoreCase(word, "atan")) {
            r = fromRadians(r);
        }
        return r;
    }
};

// ---------------------------------------------------------------------------
// 输出格式化：整数值不显示小数尾巴
// ---------------------------------------------------------------------------
std::string formatResult(double v) {
    if (v == 0.0) v = 0.0;                       // 消除 -0
    char buf[64];
    bool isInteger = (v > -1e15 && v < 1e15) &&
                     (v == std::floor(v) || v == std::ceil(v));
    if (isInteger) {
        std::snprintf(buf, sizeof(buf), "%.0f", v);
    } else {
        std::snprintf(buf, sizeof(buf), "%.12g", v);
    }
    return std::string(buf);
}

// ---------------------------------------------------------------------------
// 出错时打印带 ^ 的指示行
// ---------------------------------------------------------------------------
void printError(const std::string& input, const CalcError& e) {
    outLine("错误: " + e.msg);
    if (!input.empty()) {
        std::string caret = "  " + input + "\n  ";
        for (std::size_t i = 0; i < e.pos && i < input.size(); ++i) {
            caret += (input[i] == '\t') ? '\t' : ' ';
        }
        caret += "^\n";
        outText(caret);
    }
}

// ---------------------------------------------------------------------------
// 求值一次
// ---------------------------------------------------------------------------
ParseResult evaluate(const std::string& input, double xValue) {
    try {
        Parser p(input, xValue);
        double v = p.run();
        return ParseResult(v);
    } catch (const CalcError& e) {
        return ParseResult(e);
    } catch (...) {
        return ParseResult(CalcError("未知错误", 0));
    }
}

// ---------------------------------------------------------------------------
// 帮助
// ---------------------------------------------------------------------------
void printHelp() {
    outLine("可用内容：");
    outLine("  运算符   +  -  *  /  %(取模)  ^(乘方)  ( )");
    outLine("  函数     sqrt sin cos tan asin acos atan ln log(以10为底)");
    outLine("           abs exp floor ceil round");
    outLine("  简写     √x  等价于 sqrt(x)");
    outLine("  常量     pi  e        变量: x");
    outLine("  数字     3.14   .5   1e5   2.5E-3");
    outLine("  命令     help / ?              帮助");
    outLine("           deg / rad             切换角度制 / 弧度制");
    outLine("           plot f(x) a b [n]      在 [a,b] 上画 n 段字符图，默认 70");
    outLine("           quit / exit / q       退出");
    outLine("说明：乘方右结合，2^3^2 = 2^(3^2) = 512；-2^2 = -(2^2) = -4。");
    outLine("命令行：sci_calc [--deg|--rad] \"表达式\"");
    outLine("        sci_calc [--deg|--rad] --batch 文件   （逐行读表达式，一行一个结果）");
}

// ---------------------------------------------------------------------------
// plot：终端里用字符画函数图像
// ---------------------------------------------------------------------------
void runPlot(const std::string& expr, double a, double b, int cols) {
    if (cols < 10)  cols = 10;
    if (cols > 200) cols = 200;
    if (!(a < b)) {
        outLine("错误: 区间无效，需要 起点 < 终点");
        return;
    }

    const int kRows = 21;                      // 固定行数
    const int kMaxCols = 200;
    const int n = cols;

    // 纵轴标签占固定 10 列，后面紧跟 " |"，所以绘图区从第 12 列开始
    const int kLabelW = 10;
    const char* kPad = "          ";           // kLabelW 个空格

    double ys[kMaxCols];
    bool valid[kMaxCols];

    double ymin = 0.0, ymax = 0.0;
    bool anyValid = false;

    for (int i = 0; i < n; ++i) {
        double x = a + (b - a) * (double)i / (double)(n - 1);
        ParseResult r = evaluate(expr, x);
        valid[i] = r.ok && isFiniteD(r.value);
        ys[i] = r.ok ? r.value : 0.0;
        if (valid[i]) {
            if (!anyValid) { ymin = ymax = ys[i]; anyValid = true; }
            if (ys[i] < ymin) ymin = ys[i];
            if (ys[i] > ymax) ymax = ys[i];
        }
    }

    if (!anyValid) {
        outLine("错误: 该区间内没有可绘制的有效点（检查表达式与定义域）");
        return;
    }
    if (ymax - ymin < 1e-12) {                 // 常数函数：撑开区间避免除零
        ymin -= 1.0;
        ymax += 1.0;
    }

    char canvas[21][201];
    for (int r = 0; r < kRows; ++r)
        for (int c = 0; c < kMaxCols; ++c) canvas[r][c] = ' ';

    // x 轴（y = 0）
    if (ymin <= 0.0 && ymax >= 0.0) {
        double t = (ymax - 0.0) / (ymax - ymin);
        int axisRow = (int)(t * (kRows - 1) + 0.5);
        if (axisRow < 0) axisRow = 0;
        if (axisRow > kRows - 1) axisRow = kRows - 1;
        for (int c = 0; c < n; ++c) canvas[axisRow][c] = '-';
    }

    // 落点：第 i 个采样点就画在第 i 列
    for (int i = 0; i < n; ++i) {
        if (!valid[i]) continue;
        double t = (ys[i] - ymin) / (ymax - ymin);
        int row = (int)((1.0 - t) * (kRows - 1) + 0.5);
        if (row < 0) row = 0;
        if (row > kRows - 1) row = kRows - 1;
        canvas[row][i] = '*';
    }

    outFormat("f(x) = %s     x in [%s, %s]     y in [%s, %s]\n",
              expr.c_str(), formatResult(a).c_str(), formatResult(b).c_str(),
              formatResult(ymin).c_str(), formatResult(ymax).c_str());
    for (int r = 0; r < kRows; ++r) {
        // 每行都补一个结束符，保证输出不会越界读
        canvas[r][n] = '\0';

        // 纵轴只标顶/中/底三处；中间那处吸掉浮点误差，避免出现 5.55e-17 这种尾巴
        std::string label;
        if (r == 0) {
            label = formatResult(ymax);
        } else if (r == kRows / 2) {
            double mid = (ymax + ymin) / 2.0;
            if (std::fabs(mid) < 1e-12) mid = 0.0;
            label = formatResult(mid);
        } else if (r == kRows - 1) {
            label = formatResult(ymin);
        }
        // 手动补空格对齐，比用 printf 的 %10s 更好控制
        std::string row;
        for (std::size_t k = label.size(); k < (std::size_t)kLabelW; ++k) row += ' ';
        row += label;
        row += " |";
        row += canvas[r];
        outLine(row);
    }

    // 横轴：+ 号与绘图区第 0 列对齐，也就是前面空 kLabelW 列再加 " "
    std::string axis = kPad;
    axis += " +";
    axis.append((std::size_t)n, '-');
    outLine(axis);

    // 起止刻度：起点左对齐到绘图区第 0 列，终点右对齐到最后一列
    std::string startLabel = formatResult(a);
    std::string endLabel = formatResult(b);
    std::string scale = kPad;
    scale += "  ";
    scale += startLabel;
    int gap = n - (int)startLabel.size() - (int)endLabel.size() - 2;
    if (gap < 1) gap = 1;
    scale.append((std::size_t)gap, ' ');
    scale += endLabel;
    outLine(scale);
}

// ---------------------------------------------------------------------------
// 交互模式
// ---------------------------------------------------------------------------
void interactive() {
    outLine("科学计算器（纯标准库版）    输入 help 查看用法，quit 退出");
    outLine(std::string("当前角度单位: ") + (g_angleMode == kDegree ? "角度制 deg" : "弧度制 rad"));

    std::string line;
    for (;;) {
        outText(g_angleMode == kDegree ? "deg> " : "rad> ");
        std::fflush(stdout);
        if (!std::getline(std::cin, line)) {          // Ctrl+Z / Ctrl+D
            outLine("");
            break;
        }

        std::size_t b = line.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) continue;         // 空行
        std::size_t e = line.find_last_not_of(" \t\r\n");
        std::string cmd = line.substr(b, e - b + 1);

        if (equalsIgnoreCase(cmd, "quit") || equalsIgnoreCase(cmd, "exit") ||
            equalsIgnoreCase(cmd, "q")) {
            break;
        }
        if (equalsIgnoreCase(cmd, "help") || cmd == "?") { printHelp(); continue; }
        if (equalsIgnoreCase(cmd, "deg")) {
            g_angleMode = kDegree;
            outLine("已切换到角度制");
            continue;
        }
        if (equalsIgnoreCase(cmd, "rad")) {
            g_angleMode = kRadian;
            outLine("已切换到弧度制");
            continue;
        }

        // ---- plot 子命令 ----
        if (cmd.size() > 4 && equalsIgnoreCase(cmd.substr(0, 4), "plot") &&
            (cmd.size() == 4 || cmd[4] == ' ' || cmd[4] == '\t')) {
            std::string body = cmd.substr(4);
            double xStart = 0.0, xEnd = 0.0;           // 起点、终点（必填）
            double xCols = 70.0;                       // 采样点数（可选，默认 70）
            bool haveStart = false, haveEnd = false, haveCols = false;

            // 参数从右往左剥，规则：
            //   最后两个数字 token 依次是「终点」「起点」——这两个是必填的；
            //   再往左如果还有一个纯数字 token，那才是可选的「采样点数」。
            // 最多剥 3 个；一旦遇到非数字 token（也就是表达式）就停手。
            //
            // 注意：判断「是不是数字」必须用 strtod 做严格字面量判断，
            // 不能用 evaluate()——它会把 x、pi、e 也当成解析成功
            // （x 是自变量返回 0，pi/e 是常量），会把表达式本身误剥掉。
            double peeled[3] = { 0.0, 0.0, 0.0 };      // 从右往左剥到的数字
            int peeledCount = 0;

            for (int k = 0; k < 3; ++k) {
                std::size_t sp = body.find_last_not_of(" \t");
                if (sp == std::string::npos) break;
                std::size_t sp2 = body.find_last_of(" \t", sp);
                std::string tok = (sp2 == std::string::npos)
                                      ? body.substr(0, sp + 1)
                                      : body.substr(sp2 + 1, sp - sp2);

                const char* tbegin = tok.c_str();
                char* tend = 0;
                double val = std::strtod(tbegin, &tend);
                if (tend == tbegin || *tend != '\0') break;   // 不是纯数字，到此为止

                peeled[peeledCount++] = val;
                body = (sp2 == std::string::npos) ? std::string() : body.substr(0, sp2);
            }

            std::size_t eb = body.find_first_not_of(" \t");
            std::size_t ee = body.find_last_not_of(" \t");
            std::string expr = (eb == std::string::npos)
                                   ? std::string()
                                   : body.substr(eb, ee - eb + 1);

            // 按剥取个数把参数对号入座。
            // peeled[] 是从右往左装的，所以：
            //   剥到 3 个  ->  [0]=采样点数  [1]=终点  [2]=起点
            //   剥到 2 个  ->  [0]=终点      [1]=起点
            if (peeledCount >= 3) {
                xCols = peeled[0];
                xEnd = peeled[1];
                xStart = peeled[2];
                haveCols = haveStart = haveEnd = true;
            } else if (peeledCount == 2) {
                xEnd = peeled[0];
                xStart = peeled[1];
                haveStart = haveEnd = true;
            }

            // 表达式只剩纯数字时无法与「采样点数」区分，明确报错而不是猜
            bool exprIsBareNumber = false;
            if (!expr.empty()) {
                const char* q = expr.c_str();
                char* qe = 0;
                std::strtod(q, &qe);
                exprIsBareNumber = (qe != q && *qe == '\0');
            }

            if (!haveStart || !haveEnd || expr.empty()) {
                outLine("用法: plot 表达式 起点 终点 [采样点数]");
                outLine("例如: plot sin(x) 0 360 70        （省略采样点数时默认 70）");
                // 尾部数字全被当成参数剥掉了，说明表达式缺失或本身就是纯数字。
                // 纯数字表达式没法画图（也无从和参数区分），单独说明一下。
                if (peeledCount >= 2 && expr.empty()) {
                    outLine("（注意：表达式必须包含自变量 x，不能是纯数字）");
                }
                continue;
            }
            if (exprIsBareNumber) {
                outLine("表达式不能是纯数字（会与起点/终点/采样点数混淆），请包含自变量 x");
                outLine("例如: plot x 0 10 或 plot sin(x) 0 360 70");
                continue;
            }
            int cols = haveCols ? (int)xCols : 70;
            runPlot(expr, xStart, xEnd, cols);
            continue;
        }

        // ---- 普通表达式 ----
        ParseResult r = evaluate(cmd, 0.0);
        if (r.ok) {
            outLine("= " + formatResult(r.value));
        } else {
            printError(cmd, r.error);
        }
    }
    outLine("再见。");
}

// ---------------------------------------------------------------------------
// 批处理模式：从文件逐行读表达式，每行输出一条结果
//   一行输入 -> 一行输出，方便脚本比对，也避免 shell 传参破坏 UTF-8
// ---------------------------------------------------------------------------
int runBatch(const char* path) {
    const char* name = (std::strcmp(path, "-") == 0) ? 0 : path;

    // 逐行读取。
    // 这里用 std::getline 而不是 fgets + 固定缓冲区：fgets 在行超过缓冲区时
    // 会把一行截成多段，每段被当成独立表达式去求值，从而静默给出错误结果。
    // getline 按需增长，任意长度都能正确处理。
    std::ifstream file;
    if (name) {
        file.open(name, std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            outFormat("无法打开文件: %s\n", path);
            return 2;
        }
    }
#if defined(_WIN32)
    // 设成二进制模式，避免 CRT 做 CRLF 转换：
    // 输出统一用 LF，和文件模式保持一致，方便脚本和 diff 工具比对。
    _setmode(_fileno(stdout), _O_BINARY);
    if (!name) {
        _setmode(_fileno(stdin), _O_BINARY);
    }
#endif
    std::istream& in = name ? static_cast<std::istream&>(file)
                            : static_cast<std::istream&>(std::cin);

    std::string line;
    line.reserve(4096);                       // 常见行长的预分配，避免反复扩容
    int failed = 0;

    while (std::getline(in, line)) {
        // 去掉行尾的 CR（二进制模式下 LF 已被 getline 吃掉，CR 还在）
        while (!line.empty() && line[line.size() - 1] == '\r') {
            line.erase(line.size() - 1);
        }
        // 去掉首尾空白
        std::size_t b = line.find_first_not_of(" \t");
        if (b == std::string::npos) {          // 空行
            continue;
        }
        std::size_t e = line.find_last_not_of(" \t");
        std::string expr = line.substr(b, e - b + 1);

        // 以 # 开头视为注释
        if (expr[0] == '#') continue;

        // 批处理里也允许 deg / rad 切换
        if (equalsIgnoreCase(expr, "deg")) { g_angleMode = kDegree; continue; }
        if (equalsIgnoreCase(expr, "rad")) { g_angleMode = kRadian; continue; }

        ParseResult r = evaluate(expr, 0.0);
        if (r.ok) {
            outLine(formatResult(r.value));
        } else {
            outFormat("ERROR: %s\n", r.error.msg.c_str());
            ++failed;
        }
    }

    return failed > 0 ? 1 : 0;
}

}  // namespace

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    const char* expression = 0;
    const char* batchFile = 0;
    bool wantHelp = false;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (std::strcmp(a, "--rad") == 0) {
            g_angleMode = kRadian;
        } else if (std::strcmp(a, "--deg") == 0) {
            g_angleMode = kDegree;
        } else if (std::strcmp(a, "--batch") == 0) {
            // 下一个参数是文件名；没给就用 stdin
            batchFile = (i + 1 < argc) ? argv[++i] : "-";
        } else if (std::strcmp(a, "-h") == 0 || std::strcmp(a, "--help") == 0) {
            wantHelp = true;
        } else if (a[0] == '-' && a[1] != '\0' && !isDigitCh(a[1]) && a[1] != '.') {
            outFormat("未知参数: %s\n", a);
            return 2;
        } else {
            expression = a;                 // 第一个非选项参数当作表达式
        }
    }

    if (wantHelp) {
        printHelp();
        return 0;
    }

    if (batchFile) {
        return runBatch(batchFile);
    }

    if (expression) {
        ParseResult r = evaluate(expression, 0.0);
        if (r.ok) {
            outLine(formatResult(r.value));
            return 0;
        }
        printError(expression, r.error);
        return 1;
    }

    interactive();
    return 0;
}
