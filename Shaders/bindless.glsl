#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_nonuniform_qualifier : enable

#ifdef BINDLESS_CHECKS
#extension GL_EXT_debug_printf : enable
#endif

const uint sampler_type = 0, sampler_set = 0, sampler_binding = 0;
const uint texture_type = 1, texture_set = 1, texture_binding = 0;
const uint image_type = 2, image_set = 2, image_binding = 0;
const uint AS_type = 3, AS_set = 3, AS_binding = 0;

#ifdef BINDLESS_CHECK_VERSION
layout(buffer_reference, std430) buffer VersionBuffer {
    uint handles[];
} versionBuffer;
#endif

uint indexFromHandleFrom(uint handle, uint type_expected) {
    uint idx = handle & ((1 << 23) - 1);

#ifdef BINDLESS_CHECKS
    uint type = (handle >> 23) & 0x3;
    if (type != type_expected) {
        debugPrintfEXT("Invalid handle type %d, expected %d\n", type, type_expected);
        return -1;
    }

    uint valid = (handle >> 31);
    if (valid != 0) {
        debugPrintfEXT("Invalid handle %d\n", handle);
        return -1;
    }

#ifdef BINDLESS_CHECK_VERSION
    uint version = (handle >> 25) & ((1 << 6) - 1);
    uint wanted_version = (versionBuffer.handles[idx / 4] >> ((idx % 4) * 8)) & 0xFF;

    if (version != wanted_version) {
        debugPrintfEXT("Invalid handle version %d, expected %d\n", version, wanted_version);
        return -1;
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

#define TEXTURE(type, name) \
    layout(set = texture_set, binding = texture_binding) uniform type name[];

///////// STORAGE IMAGES ///////////

uint imageIdx(uint handle) {
    return indexFromHandleFrom(handle, image_type);
}

#define STORAGE_IMAGE(type, name, format, attributes) \
    layout(set = image_set, binding = image_binding, format) attributes uniform type name[];

#define STORAGE_IMAGE_NOFORMAT(type, name, attributes) \
    layout(set = image_set, binding = image_binding) attributes uniform type images_ ## name[];

///////// ACCELERATION STRUCTURES ///////////

#ifdef USE_RAYTRACING
uint AccStructIdx(uint handle) {
    return indexFromHandleFrom(handle, AS_type);
}

layout(set = acceleration_structure_set, binding = acceleration_structure_binding) 
    uniform accelerationStructureEXT AccStructs[];

accelerationStructureEXT toAccStruct(uint handle) {
    return AccStructs[AccStructIdx(handle)];
}
#endif