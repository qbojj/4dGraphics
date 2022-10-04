#pragma once

#include <vector>
#include <stdint.h>

#include <glslang/Public/ShaderLang.h>
std::vector<uint32_t> compileShaderToSPIRV(EShLanguage stage, const char* shaderSource, const char* fileName);
std::vector<uint32_t> compileShaderToSPIRVFromFile(EShLanguage stage, const char* fileName);

EShLanguage stageFromFilename( const char *fileName );

// cache
std::vector<uint32_t> getShaderOrGenerate( EShLanguage stage, const char *shaderSource );