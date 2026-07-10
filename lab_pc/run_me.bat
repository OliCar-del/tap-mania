@echo off
setlocal
cd /d "%~dp0"

:: Check if Python is installed
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] Python not detected. Installing automatically...
    goto install_python
) else (
    echo [OK] Python environment detected.
    goto install_requirements
)

:install_python
set PYTHON_URL=https://www.python.org/ftp/python/3.11.9/python-3.11.9-amd64.exe
set INSTALLER=python_installer.exe
echo [+] Downloading Python installer...
powershell -Command "Invoke-WebRequest -Uri %PYTHON_URL% -OutFile %INSTALLER%"
if not exist "%INSTALLER%" (
    echo [X] Download failed. Please check your internet connection or install Python manually.
    pause
    exit /b
)

echo [+] Installing Python silently (this may take a few minutes)...
start /wait "" "%INSTALLER%" /quiet InstallAllUsers=1 PrependPath=1 Include_test=0

:: Reload PATH environment variable so the current CMD recognizes the new path
for /f "tokens=2*" %%A in ('reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v Path 2^>nul') do set "PATH=%%B"

python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [X] Python installation failed. Please reopen this script manually.
    pause
    exit /b
)
echo [OK] Python successfully installed.
goto install_requirements

:install_requirements
echo [+] Installing required Python modules...
pip install pyserial psutil
if %errorlevel% neq 0 (
    echo [X] Module installation failed. Please check your internet connection.
    pause
    exit /b
)

:run_script
echo [+] Launching tpmania Configuration Tool...
python "%~dp03800gui.py"
pause
exit /b
