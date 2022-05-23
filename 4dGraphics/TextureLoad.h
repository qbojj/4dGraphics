#pragma once

#include "CommonUtility.h"
#include "GLId.h"

enum class TextureCompressionModel { NoCompression, Weak, Medium, Strong, Default = Weak };
GLTexId CreateEmptyTexture();
GLTexId CreateTexture( const char *Filename, TextureCompressionModel compr = TextureCompressionModel::Default );
GLTexId CreateTextureDefault( const char *Filename, const glm::u8 def[4], TextureCompressionModel compr = TextureCompressionModel::Default );