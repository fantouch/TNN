
@echo off

set CUR_DIR=%~dp0
set OUT_FILE_NAME=%CUR_DIR%\directx_kernels.cc
set TNN_HLSL_PATH=%1
set COMPILE_FLAGS=/E CSMain /T cs_5_0 /Fh
set KERNEL_DIR=%CUR_DIR%\kernels

echo #include ^<map^> > %OUT_FILE_NAME%
echo #include ^<string^> >> %OUT_FILE_NAME%
echo #include "directx_kernels.h" >> %OUT_FILE_NAME%
echo typedef unsigned char BYTE; >> %OUT_FILE_NAME%
echo namespace TNN_NS { >> %OUT_FILE_NAME%
echo namespace directx { >> %OUT_FILE_NAME%

for %%f in (%KERNEL_DIR%\*.hlsl) do (
    if "%%~xf"==".hlsl" (
      echo #include "kernels/%%~nf.h" >> %OUT_FILE_NAME%
      fxc %%f %COMPILE_FLAGS% /Vn g_%%~nf
    )
)

:: for get_kernel_map() function

echo std::map^<std::string, const unsigned char *^> ^& get_kernel_map^(^){ >> %OUT_FILE_NAME%
echo     static std::map^<std::string, const unsigned char *^> s_kernel_map; >> %OUT_FILE_NAME%

for %%f in (%KERNEL_DIR%\*.hlsl) do (
    echo     s_kernel_map["%%~nf"] = g_%%~nf; >> %OUT_FILE_NAME%
)

echo     return s_kernel_map; >> %OUT_FILE_NAME%
echo } >> %OUT_FILE_NAME%

:: for get_kernel_size_map() function
echo std::map^<std::string, size_t^> ^& get_kernel_size_map^(^){ >> %OUT_FILE_NAME%
echo     static std::map^<std::string, size_t^> s_kernel_size_map; >> %OUT_FILE_NAME%

for %%f in (%KERNEL_DIR%\*.hlsl) do (
    echo     s_kernel_size_map["%%~nf"] = sizeof^(g_%%~nf^); >> %OUT_FILE_NAME%
)
echo     return s_kernel_size_map; >> %OUT_FILE_NAME%
echo }  >> %OUT_FILE_NAME%

echo } } >> %OUT_FILE_NAME%

