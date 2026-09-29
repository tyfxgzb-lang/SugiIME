# 导入 SugiIME 自签名测试证书到 Trusted Root 和 Trusted Publisher。
# 必须在安装前运行一次，否则 Windows 会拦截未受信任的签名，Server uiAccess 不生效。
# 以管理员身份运行（脚本内会自动提权）。

$ErrorActionPreference = 'Stop'

# 自动提权
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Start-Process pwsh -ArgumentList '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $PSCommandPath -Verb RunAs
    exit
}

$cerPath = Join-Path $PSScriptRoot 'SugiIME-Test.cer'
if (-not (Test-Path -LiteralPath $cerPath -PathType Leaf)) {
    throw "证书文件不存在：$cerPath`n请确认从 artifact zip 解压后证书与本脚本在同一目录。"
}

Write-Host "正在导入测试证书：$cerPath"

# 导入到 Trusted Root Certification Authorities（让系统信任此证书）
& certutil -addstore Root $cerPath
if ($LASTEXITCODE -ne 0) { throw "导入 Root 失败，退出码：$LASTEXITCODE" }

# 导入到 Trusted Publishers（让系统信任此证书签发的程序，安装时不弹 SmartScreen 警告）
& certutil -addstore TrustedPublisher $cerPath
if ($LASTEXITCODE -ne 0) { throw "导入 TrustedPublisher 失败，退出码：$LASTEXITCODE" }

Write-Host ""
Write-Host "测试证书已成功导入。现在可以运行 install.ps1 安装 SugiIME。"
Read-Host "按回车键退出"
