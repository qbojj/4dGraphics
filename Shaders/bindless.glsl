#ifndef BINDLESS_
#define BINDLESS_

#ifndef VULKAN
#error "This file is only for Vulkan"
#endif

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_texture_shadow_lod : require
#extension GL_EXT_debug_printf : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_8bit_storage : require

const uint sampler_type = 0, sampler_set = 0, sampler_binding = 0;
const uint texture_type = 1, texture_set = 0, texture_binding = 1;
const uint image_type = 2, image_set = 0, image_binding = 2;
const uint AS_type = 3, AS_set = 0, AS_binding = 3;

// first number not used by bindless framework
const uint user_set = 1;

#ifdef BINDLESS_CHECK_BUFFER
layout(buffer_reference, scalar) buffer VersionBuffer {
    uint8_t handles[];
};

layout(set = 0, binding = 4, scalar) uniform AllVersionBuffers {
    uint maxHandles[4];
    VersionBuffer versions[4];
} bindlessVersionBuffers;
#endif

uint indexFromHandleFrom(uint handle, uint type_expected) {
    uint idx = handle & ((1 << 23) - 1);

#ifdef BINDLESS_CHECKS
    uint valid = (handle >> 31);
    if (valid == 0) {
        debugPrintfEXT("Invalid handle %d\n", handle);
        return 0;
    }

    uint type = (handle >> 23) & 0x3;
    if (type != type_expected) {
        debugPrintfEXT("Invalid handle type %d, expected %d\n", type, type_expected);
        return 0;
    }

#ifdef BINDLESS_CHECK_BUFFER
    uint maxIdx = bindlessVersionBuffers.maxHandles[type_expected];
    VersionBuffer vb = bindlessVersionBuffers.versions[type_expected];

    if (idx >= maxIdx) {
        debugPrintfEXT("Invalid handle index %d, max %d\n", idx, maxIdx);
        return 0;
    }

    uint version = (handle >> 25) & ((1 << 6) - 1);
    uint version_expected = uint(vb.handles[idx]);

    if (version != version_expected) {
        debugPrintfEXT("Invalid handle version %d, expected %d\n", version, version_expected);
        return 0;
    }
#endif
#endif

    return idx;
}

///////// SAMPLERS ///////////

uint samplerIdx(uint handle) {
    return indexFromHandleFrom(handle, sampler_type);
}

layout(set = sampler_set, binding = sampler_binding) uniform sampler samplers[];
layout(set = sampler_set, binding = sampler_binding) uniform samplerShadow samplersShadow[];
sampler toSampler(uint handle) {
    return samplers[samplerIdx(handle)];
}

samplerShadow toSamplerShadow(uint handle) {
    return samplersShadow[samplerIdx(handle)];
}

///////// TEXTURES ///////////

uint textureIdx(uint handle) {
    return indexFromHandleFrom(handle, texture_type);
}

#define DEF_SAMPLE_TEXTURE(Pref, type, arr, sampType, res, UV) \
    res Pref ## type(uint tex_handle, uint sampler_handle, UV uv) { \
        return texture(sampType(arr[textureIdx(tex_handle)], toSampler(sampler_handle)), uv); \
    }

#define DEF_SAMPLE_TEXTURE_BIAS(Pref, type, arr, sampType, res, UV) \
    res Pref ## Bias ## type(uint tex_handle, uint sampler_handle, UV uv, float bias) { \
        return texture(sampType(arr[textureIdx(tex_handle)], toSampler(sampler_handle)), uv, bias); \
    }

#define DEF_SAMPLE_TEXTURE_LOD(Pref, type, arr, sampType, res, UV) \
    res Pref ## Lod ## type(uint tex_handle, uint sampler_handle, UV uv, float lod) { \
        return textureLod(sampType(arr[textureIdx(tex_handle)], toSampler(sampler_handle)), uv, lod); \
    }

#define DEF_SAMPLE_TEXTURE_GRAD(Pref, type, arr, sampType, res, UV, D) \
    res Pref ## Grad ## type(uint tex_handle, uint sampler_handle, UV uv, D ddx, D ddy) { \
        return textureGrad(sampType(arr[textureIdx(tex_handle)], toSampler(sampler_handle)), uv, ddx, ddy); \
    }

#define TEXTURE_IMPL(Pref, type, arr, sampType, res, UV, D) \
    DEF_SAMPLE_TEXTURE(Pref, type, arr, sampType, res, UV) \
    DEF_SAMPLE_TEXTURE_BIAS(Pref, type, arr, sampType, res, UV) \
    DEF_SAMPLE_TEXTURE_LOD(Pref, type, arr, sampType, res, UV) \
    DEF_SAMPLE_TEXTURE_GRAD(Pref, type, arr, sampType, res, UV, D)

#define TEXTURE(type, UV, D, sUV) \
    layout(set = texture_set, binding = texture_binding) uniform texture ## type textures ## type[]; \
    TEXTURE_IMPL(Texture, type, textures ## type, sampler ## type, vec4, UV, D) \
    TEXTURE_IMPL(Texture, type ## Shadow, textures ## type, sampler ## type ## Shadow, float, sUV, D)

TEXTURE(1D, float, float, vec3)
TEXTURE(1DArray, vec2, float, vec3)
TEXTURE(2D, vec2, vec2, vec3)
TEXTURE(2DArray, vec3, vec2, vec4)
TEXTURE(Cube, vec3, vec3, vec4)

///////// STORAGE IMAGES ///////////

uint imageIdx(uint handle) {
    return indexFromHandleFrom(handle, image_type);
}

#define STORAGE_IMAGE(type, name, format, attributes) \
    layout(set = image_set, binding = image_binding, format) attributes uniform type name[];

#define STORAGE_IMAGE_NOFORMAT(type, name, attributes) \
    layout(set = image_set, binding = image_binding) attributes uniform type name[];


///////// ACCELERATION STRUCTURES ///////////

#ifdef RAYTRACING
uint AccStructIdx(uint handle) {
    return indexFromHandleFrom(handle, AS_type);
}

layout(set = AS_set, binding = AS_binding) uniform accelerationStructureEXT AccStructs[];

accelerationStructureEXT toAccStruct(uint handle) {
    return AccStructs[AccStructIdx(handle)];
}
#endif

#endif // BINDLESS_