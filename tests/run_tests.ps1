# 严格对照测试：用 --batch 模式，一行输入 -> 一行输出，一一比对
# 全程走 UTF-8 文件，绕开 PowerShell 传参对 √ / π 的破坏
#
# 路径自动定位，不依赖任何绝对路径：脚本在 <项目>/tests/ 下，
# 所以项目根目录就是它的上一级。克隆到任何位置都能直接跑。

$testsDir = $PSScriptRoot
$d        = Split-Path $testsDir -Parent
$exe      = Join-Path $d "sci_calc.exe"
$caseFile = Join-Path $testsDir "_cases.txt"
$outFile  = Join-Path $testsDir "_results.txt"

if (-not (Test-Path $exe)) {
    Write-Output "ERROR: sci_calc.exe not found at: $exe"
    Write-Output "Build it first, from the project root:"
    Write-Output "    Windows + VS2022 : run build.bat"
    Write-Output "    g++ / clang++    : g++ -std=c++17 -O2 -o sci_calc scientific_calc.cpp"
    exit 1
}

# [输入, 期望]；期望以 "ERROR:" 开头表示只比对错误类型
$cases = @(
  # ===== 基本运算 =====
  @('1+2',              '3'),
  @('10-3',             '7'),
  @('6*7',              '42'),
  @('9/4',              '2.25'),
  @('2+3*4',            '14'),
  @('(2+3)*4',          '20'),
  @('((1+2)*(3+4))-5',  '16'),
  @('((((1+2))))',      '3'),
  @('0.1+0.2',          '0.3'),
  @('7%3',              '1'),
  @('10%3.5',           '3'),
  @('1+2*3^2-4/2',      '17'),
  @('1/3',              '0.333333333333'),
  @('0',                '0'),
  @('-0',               '0'),
  @('--5',              '5'),
  @('  1 + 2 * 3  ',    '7'),
  # ===== 乘方 / 一元 =====
  @('2^10',             '1024'),
  @('2^3^2',            '512'),
  @('-2^2',             '-4'),
  @('2^-3',             '0.125'),
  @('(-2)^2',           '4'),
  @('-3+5',             '2'),
  @('3--2',             '5'),
  @('+5',               '5'),
  @('2^0.5',            '1.41421356237'),
  @('2^0',              '1'),
  @('(-2)^3',           '-8'),
  # ===== UTF-8：√ 与 π =====
  @('√25',              '5'),
  @('√(9+7)',           '4'),
  @('2*√9',             '6'),
  @('√2',               '1.41421356237'),
  @('√(2^4)',           '4'),
  @('π',                '3.14159265359'),
  @('2*π',              '6.28318530718'),
  @('√-4',              'ERROR: 对负数开平方'),
  @('2π',               "ERROR: 数字 2 后面不能直接接 'π'"),
  # ===== 函数 =====
  @('sqrt(16)',         '4'),
  @('sqrt(2)',          '1.41421356237'),
  @('sin(30)',          '0.5'),
  @('cos(60)',          '0.5'),
  @('tan(45)',          '1'),
  @('asin(0.5)',        '30'),
  @('acos(0.5)',        '60'),
  @('atan(1)',          '45'),
  @('ln(e)',            '1'),
  @('log(1000)',        '3'),
  @('abs(-7)',          '7'),
  @('abs(-0.5)',        '0.5'),
  @('floor(2.9)',       '2'),
  @('floor(-2.5)',      '-3'),
  @('ceil(2.1)',        '3'),
  @('ceil(-2.5)',       '-2'),
  @('round(2.5)',       '3'),
  @('round(-2.5)',      '-3'),
  @('exp(0)',           '1'),
  @('pi',               '3.14159265359'),
  @('PI',               '3.14159265359'),
  @('e',                '2.71828182846'),
  @('SQRT(9)',          '3'),
  @('sin(30)+cos(60)',  '1'),
  @('sqrt(sqrt(16))',   '2'),
  @('2*sin(30)^2+2*cos(30)^2', '2'),
  @('ln(exp(2))',       '2'),
  # ===== 数字字面量 =====
  @('1e3',              '1000'),
  @('2.5E-2',           '0.025'),
  @('.5+.25',           '0.75'),
  @('1e-2',             '0.01'),
  # ===== 错误处理 =====
  @('1/0',              'ERROR: 除以零'),
  @('5%0',              'ERROR: 对零取模'),
  @('1+@',              "ERROR: 非法字符 '@'"),
  @('(1+2',             "ERROR: 缺少右括号 ')'"),
  @('1+2)',             "ERROR: 多余的内容 ')'"),
  @('1+',               'ERROR: 表达式意外结束，缺少操作数'),
  @('foo(1)',           "ERROR: 未知的名称 'foo'"),
  @('sqrt 2',           'ERROR: 函数 sqrt 后面需要括号'),
  @('sqrt(-1)',         'ERROR: 函数 sqrt 在 x=-1 处无定义（定义域错误）'),
  @('asin(2)',          'ERROR: 函数 asin 在 x=2 处无定义'),
  @('9^9^9',            'ERROR: 乘方结果溢出'),
  @('(-8)^0.5',         'ERROR: 负数底数配非整数指数，结果不是实数'),
  @('0^-1',             'ERROR: 0 的负数次幂（相当于除以零）'),
  @('2pi',              "ERROR: 数字 2 后面不能直接接 'p'"),
  @('1e',               "ERROR: 数字 1 后面不能直接接 'e'"),
  @('1 2',              "ERROR: 多余的内容 '2'"),
  @('3.4.5',            "ERROR: 多余的内容 '.'"),
  @('sin()',            'ERROR: 函数 sin 缺少参数'),
  @('((1)',             "ERROR: 缺少右括号 ')'"),
  @('1++',              'ERROR: 表达式意外结束，缺少操作数'),
  # ===== 角度单位切换（批处理里也支持）=====
  @('rad',              ''),
  @('sin(pi/2)',        '1'),
  @('asin(1)',          '1.57079632679'),
  @('deg',              ''),
  @('sin(30)',          '0.5')
)

$inLines = @()
foreach ($c in $cases) { $inLines += $c[0] }
[System.IO.File]::WriteAllLines($caseFile, $inLines, (New-Object System.Text.UTF8Encoding($false)))

& cmd /c "`"$exe`" --batch `"$caseFile`" > `"$outFile`" 2>&1"
$exitCode = $LASTEXITCODE

$results = @([System.IO.File]::ReadAllLines($outFile, [System.Text.Encoding]::UTF8))

Write-Output "batch exit code: $exitCode"
Write-Output "cases: $($cases.Count)   output lines: $($results.Count)"
Write-Output ""

$pass = 0; $fail = 0; $failList = @()
$ri = 0
foreach ($c in $cases) {
  $input = $c[0]
  $expect = $c[1]

  # deg / rad 是切换指令，不产出结果行
  if ($input -eq 'deg' -or $input -eq 'rad') { $pass++; continue }

  if ($ri -ge $results.Count) {
    $fail++; $failList += "$input -> no output"
    Write-Output ("  FAIL  {0,-28} no output" -f $input)
    continue
  }
  $got = $results[$ri].Trim()
  $ri++

  $ok = if ($expect.StartsWith('ERROR:')) {
          $got.StartsWith('ERROR:') -and $got.Contains($expect.Substring(6).Trim())
        } else {
          $got -eq $expect
        }

  if ($ok) {
    $pass++
    Write-Output ("  PASS  {0,-28} -> {1}" -f $input, $got)
  } else {
    $fail++
    $failList += ("{0,-28} want {1}  got {2}" -f $input, $expect, $got)
    Write-Output ("  FAIL  {0,-28} want {1}  got {2}" -f $input, $expect, $got)
  }
}

Write-Output ""
Write-Output "============ $pass passed / $fail failed ============"
if ($failList.Count -gt 0) {
  Write-Output ""
  Write-Output "failures:"
  $failList | ForEach-Object { "  - $_" }
}
