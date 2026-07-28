@echo off
SETLOCAL EnableDelayedExpansion

echo ===================================================
echo   FINAL PORTABLE SETUP: BMad + OpenCode + Ollama
echo ===================================================

:: 1. Navigate to working directory
cd /d V:\Bmad

:: 2. Initialize local project if not exists
if not exist "package.json" (
    echo [1/4] Initializing local project...
    call npm init -y
)

:: 3. Clear NPM Cache
echo [2/4] Cleaning NPM cache...
call npm cache clean --force

:: 4. Local Install of tools AND the AI SDK
echo [3/4] Installing tools and AI SDK locally...
call npm install opencode-ai@latest bmad-method@latest @ai-sdk/openai-compatible@latest --legacy-peer-deps
call npm install ollama-ai-provider-v2 --legacy-peer-deps

:: 5. Initialize BMad (Automated non-interactive installation)
echo [4/4] Starting BMad Method installation...
call npx bmad-method@next install --directory V:\Bmad --modules bmm --tools opencode --yes

echo ===================================================
echo   SETUP COMPLETE!
echo ===================================================
echo 1. Ensure Ollama is running (ollama serve).
echo 2. Ensure you have the model: ollama pull qwen3.5:35b
echo 3. Run: run.bat
echo ===================================================
pause