param(
    [ValidateSet("all", "clean")]
    [string]$Target = "all",
    [string]$ModelBlob
)

$ErrorActionPreference = "Stop"

function Require-Command([string]$Name) {
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "$Name not found in PATH"
    }
}

function Invoke-Checked([string]$Exe, [string[]]$Args) {
    & $Exe @Args
    if ($LASTEXITCODE -ne 0) {
        throw "$Exe failed with exit code $LASTEXITCODE"
    }
}

function Get-ObjPath([string]$SrcPath, [string]$SrcRoot, [string]$BuildRoot) {
    $rel = $SrcPath.Substring($SrcRoot.Length)
    $rel = $rel.TrimStart([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
    $dir = Split-Path $rel -Parent
    $base = [System.IO.Path]::GetFileNameWithoutExtension($rel)
    if ([string]::IsNullOrEmpty($dir)) {
        return (Join-Path $BuildRoot ($base + ".o"))
    }
    return (Join-Path $BuildRoot (Join-Path $dir ($base + ".o")))
}

$root = $PSScriptRoot
if ([string]::IsNullOrEmpty($root)) {
    $root = (Get-Location).Path
}

$srcDir = Join-Path $root "src"
$buildDir = Join-Path $root "build"
$kernelBin = Join-Path $root "kernel.bin"
$ldScript = Join-Path $root "linker.ld"
$assetsDir = Join-Path $root "assets"

if ($Target -eq "clean") {
    if (Test-Path $buildDir) {
        Remove-Item -Recurse -Force $buildDir
    }
    if (Test-Path $kernelBin) {
        Remove-Item -Force $kernelBin
    }
    exit 0
}

Require-Command "i686-elf-gcc"
Require-Command "nasm"
Require-Command "i686-elf-objcopy"

if (-not (Test-Path $ldScript)) {
    throw "linker.ld not found"
}

if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
}

$asmSrcs = Get-ChildItem -Path $srcDir -Recurse -Filter "*.s"
$cSrcs = Get-ChildItem -Path $srcDir -Recurse -Filter "*.c"

$objPaths = @()

$asFlags = @("-f", "elf32")
$cFlags = @("-ffreestanding", "-O2", "-Wall", "-Wextra", "-m32", "-fno-pie", "-fno-stack-protector", "-nostdlib", "-nostdinc", "-Iinclude")

foreach ($src in $asmSrcs) {
    $obj = Get-ObjPath $src.FullName $srcDir $buildDir
    $objDir = Split-Path $obj -Parent
    if (-not (Test-Path $objDir)) {
        New-Item -ItemType Directory -Force -Path $objDir | Out-Null
    }
    Invoke-Checked "nasm" ($asFlags + @($src.FullName, "-o", $obj))
    $objPaths += $obj
}

foreach ($src in $cSrcs) {
    $obj = Get-ObjPath $src.FullName $srcDir $buildDir
    $objDir = Split-Path $obj -Parent
    if (-not (Test-Path $objDir)) {
        New-Item -ItemType Directory -Force -Path $objDir | Out-Null
    }
    Invoke-Checked "i686-elf-gcc" ($cFlags + @("-c", $src.FullName, "-o", $obj))
    $objPaths += $obj
}

$modelBlobPath = $ModelBlob
if ([string]::IsNullOrEmpty($modelBlobPath)) {
    $candidateA = Join-Path $assetsDir "smollm-135m.gguf"
    $candidateB = Join-Path $assetsDir "SmolLM2-135M-Instruct-Q4_K_M.gguf"
    if (Test-Path $candidateA) {
        $modelBlobPath = $candidateA
    } elseif (Test-Path $candidateB) {
        $modelBlobPath = $candidateB
    }
}

if (-not [string]::IsNullOrEmpty($modelBlobPath)) {
    if (-not (Test-Path $modelBlobPath)) {
        throw "Model blob not found: $modelBlobPath"
    }
    $modelObj = Join-Path $buildDir "model.o"
    Invoke-Checked "i686-elf-objcopy" @(
        "-I", "binary",
        "-O", "elf32-i386",
        "-B", "i386",
        "--rename-section", ".data=.ai_model,alloc,load,readonly,data,contents",
        $modelBlobPath,
        $modelObj
    )
    $objPaths += $modelObj
}

$ldFlags = @("-T", $ldScript, "-ffreestanding", "-O2", "-nostdlib", "-Wl,--build-id=none")
Invoke-Checked "i686-elf-gcc" ($ldFlags + @("-o", $kernelBin) + $objPaths)
