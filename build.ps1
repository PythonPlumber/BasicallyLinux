param(
    [ValidateSet("all", "clean", "iso")]
    [string]$Target = "all",
    [string]$ModelBlob
)

$ErrorActionPreference = "Stop"

function Require-Command([string]$Name) {
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "$Name not found in PATH"
    }
}

function Find-FirstCommand([string[]]$Names) {
    foreach ($name in $Names) {
        if (Get-Command $name -ErrorAction SilentlyContinue) {
            return $name
        }
    }
    throw ("None of the following commands were found in PATH: " + ($Names -join ", "))
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

if ($Target -eq "clean") {
    if (Test-Path $buildDir) {
        Remove-Item -Recurse -Force $buildDir
    }
    if (Test-Path $kernelBin) {
        Remove-Item -Force $kernelBin
    }
    exit 0
}

$cc = Find-FirstCommand @("i686-elf-gcc", "i686-linux-gnu-gcc")
$objcopy = Find-FirstCommand @("i686-elf-objcopy", "i686-linux-gnu-objcopy")
Require-Command "nasm"

if (-not (Test-Path $ldScript)) {
    throw "linker.ld not found"
}

if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
}

$asmSrcs = Get-ChildItem -Path $srcDir -Recurse -Filter "*.asm"
$cSrcs = Get-ChildItem -Path $srcDir -Recurse -Filter "*.c"

$objPaths = @()

$asFlags = @("-f", "elf32")
$cFlags = @("-ffreestanding", "-O2", "-Wall", "-Wextra", "-m32", "-fno-pie", "-fno-PIC", "-fno-stack-protector", "-nostdlib", "-nostdinc", "-msse", "-msse2", "-Iinclude")
if ([string]::IsNullOrEmpty($modelBlobPath)) {
    $cFlags += "-DNO_AI_MODEL"
}

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
    Invoke-Checked $cc ($cFlags + @("-c", $src.FullName, "-o", $obj))
    $objPaths += $obj
}

if (-not [string]::IsNullOrEmpty($modelBlobPath)) {
    if (-not (Test-Path $modelBlobPath)) {
        throw "Model blob not found: $modelBlobPath"
    }
    $modelObj = Join-Path $buildDir "model.o"
    Invoke-Checked $objcopy @(
        "-I", "binary",
        "-O", "elf32-i386",
        "-B", "i386",
        "--rename-section", ".data=.ai_model,alloc,load,readonly,data,contents",
        $modelBlobPath,
        $modelObj
    )
    $objPaths += $modelObj
}

$ldFlags = @("-T", $ldScript, "-ffreestanding", "-O2", "-nostdlib", "-m32", "-no-pie", "-Wl,-z,noexecstack", "-Wl,--build-id=none")
Invoke-Checked $cc ($ldFlags + @("-o", $kernelBin) + $objPaths)

if ($Target -eq "iso") {
    $isoRoot = Join-Path $buildDir "isodir"
    $isoBoot = Join-Path $isoRoot "boot"
    $isoGrub = Join-Path $isoBoot "grub"
    $grubCfg = Join-Path $isoGrub "grub.cfg"
    $isoPath = Join-Path $buildDir "basicallylinux.iso"

    Require-Command "grub-mkrescue"
    Require-Command "xorriso"

    New-Item -ItemType Directory -Force -Path $isoGrub | Out-Null
    Copy-Item -Force $kernelBin (Join-Path $isoBoot "kernel.bin")
    if (-not [string]::IsNullOrEmpty($modelBlobPath)) {
        Copy-Item -Force $modelBlobPath (Join-Path $isoBoot ([System.IO.Path]::GetFileName($modelBlobPath)))
    }

    $lines = @(
        "set timeout=5",
        "set default=0",
        "menuentry `"BasicallyLinux`" {",
        "    multiboot /boot/kernel.bin"
    )
    if (-not [string]::IsNullOrEmpty($modelBlobPath)) {
        $modelName = [System.IO.Path]::GetFileName($modelBlobPath)
        $lines += "    module /boot/$modelName `"ai_model`""
    }
    $lines += "    boot"
    $lines += "}"
    Set-Content -Path $grubCfg -Value $lines -Encoding ASCII

    Invoke-Checked "grub-mkrescue" @("-o", $isoPath, $isoRoot)
}
