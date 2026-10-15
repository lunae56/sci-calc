# 把这份内容粘贴到 GitHub 的 Release 说明框里
# 网页新建 Release：https://github.com/lunae56/sci-calc/releases/new
#   - Choose a tag → 选 v1.0.0（若列表里没有，手动输入 v1.0.0 → Create new tag）
#   - Release title → 填下面这行
#   - Describe this release → 粘贴下面正文
#   - Attach binaries → 把 sci_calc.exe 拖进去

## Release title（标题栏就填这一行）

v1.0.0 — 首个正式版本

## Describe this release（正文，从下一行开始全部粘贴）

纯 C++ 标准库实现的控制台科学计算器，零外部依赖。

## 亮点

- **零依赖**：只用 C++ 标准库，不使用 `vector` / `map` 等容器，解析器为手写递归下降
- **可执行文件免安装**：MSVC 运行时已静态链接，下载后双击即可运行，无需安装任何运行库
- **93 个测试用例全部通过**，MSVC `/W4` 零警告编译

## 功能

**运算符**：`+` `-` `*` `/` `%`（取模） `^`（乘方） `(` `)`，一元正负号

**函数**：`sqrt` `sin` `cos` `tan` `asin` `acos` `atan` `ln` `log` `abs` `exp` `floor` `ceil` `round`

**其他语法**：
- `√x` 简写，等价于 `sqrt(x)`
- 常量 `pi` / `PI` / `π`、`e`
- 科学计数法 `1e5`、`2.5E-3`
- 角度制 / 弧度制切换（`--deg` / `--rad`，或交互中输入 `deg` / `rad`）

**优先级**：乘方右结合（`2^3^2` = 512），一元负号低于乘方（`-2^2` = -4）

**plot 子命令**：在终端用字符画函数图像，不需要任何图形库

```
deg> plot sin(x) 0 360 60
0.999645611123 |            ******
           |          **      **
           |        **          **
         0 |*----------------------------------------------------------*
           |                              *                           *
-0.999645611123 |                                          ******
           +------------------------------------------------------------
            0                                                      360
```

## 错误处理

除零、非法字符、括号不匹配、缺操作数、函数缺括号/缺参数、负数开平方、定义域错误、乘方溢出、数字后接字母等，**均报错并指出出错位置**：

```
deg> 2π
错误: 数字 2 后面不能直接接 'π'，请加运算符（例如 2*π）
  2π
   ^
```

退出码：正常 `0`，表达式错误 `1`，参数错误 `2`

## 使用方法

```bash
sci_calc                       # 交互模式（默认角度制）
sci_calc "2+3*4"               # 算一个表达式
sci_calc --rad "sin(pi/2)"     # 弧度模式
sci_calc --batch 表达式.txt     # 批处理：逐行读表达式，一行一个结果
sci_calc --help                # 帮助
```

交互模式命令：`help` / `deg` / `rad` / `plot` / `quit`

## 从源码编译

只需一个源文件，任选编译器：

```bash
# MSVC
cl /std:c++17 /utf-8 /W4 /O2 /EHsc scientific_calc.cpp

# g++ / clang++
g++ -std=c++17 -O2 -Wall -Wextra -o sci_calc scientific_calc.cpp
```

Windows 上装了 VS2022 的话，直接双击 `build.bat` 即可。

## 测试

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests\run_tests.ps1
```

93 个用例，覆盖基本运算、优先级与结合性、全部函数、UTF-8 字符、数字字面量、20 种错误情况、角度单位切换。

## 附件说明

| 文件 | 说明 |
| --- | --- |
| `sci_calc.exe` | Windows x64 可执行文件（330 KB），静态链接运行时，下载即用 |
| `Source code (zip)` | GitHub 自动生成的源码包 |

> 在 Windows 上如果中文显示成乱码，请确认控制台代码页——程序已内置编码转换，正常情况下不需要任何设置。若显示为方块，请在控制台标题栏右键 →「属性」→「字体」选用 Consolas 等 TrueType 字体。

## 许可证

[MIT](https://github.com/lunae56/sci-calc/blob/main/LICENSE) © 2026 lunae56
