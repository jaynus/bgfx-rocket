#include "RocketBgfxInterface.hpp"

#include <stdexcept>
#include <iostream>
#include <bx/fpumath.h>

#include <stb_image.h>

using namespace space::Render;

bgfx::VertexDecl space::Render::RocketBgfxInterface::RocketVertexData::ms_decl;

RocketBgfxInterface::RocketBgfxInterface(int viewNumber, float _width, float _height, bool configureView)
    : _currentTextureIndex(1), _viewNumber(viewNumber), _width(_width), _height(_height)
{
    RocketVertexData::init();
    if (configureView) setViewParameters();

    _dynamicVertexBuffer = bgfx::createDynamicVertexBuffer(MAX_DYNAMIC_VERTICES, RocketVertexData::ms_decl);
    _dynamicIndexBuffer = bgfx::createDynamicIndexBuffer(MAX_DYNAMIC_INDICES, BGFX_BUFFER_INDEX32);
}
RocketBgfxInterface::RocketBgfxInterface(int viewNumber, bgfx::ProgramHandle colorShader, bgfx::ProgramHandle textureShader, float _width, float _height, bool configureView)
    : _colorShader(colorShader), _textureShader(textureShader), _width(_width), _height(_height), _currentTextureIndex(1), _viewNumber(viewNumber), _scissorParams({ false, 0, 0, 0, 0 })
{
    RocketVertexData::init();
    if (configureView) setViewParameters();

    _dynamicVertexBuffer = bgfx::createDynamicVertexBuffer(MAX_DYNAMIC_VERTICES, RocketVertexData::ms_decl);
    _dynamicIndexBuffer = bgfx::createDynamicIndexBuffer(MAX_DYNAMIC_INDICES, BGFX_BUFFER_INDEX32);
}
//
// We render the librocket UI into a seperate vview, thus keeping it seperated from any other rendering
// This is a managing helper function to compile the orthographic 2d graphics view. 
// You could skip this call (constructor configureView=false, and manage the view transformations yourself - thus allowing for view/framebuffer RTT UI elements.
void RocketBgfxInterface::setViewParameters(int viewNumber) {
    // Assign the view 
    float orthoView[16] = { 0 };
    float identity[16] = { 0 };
    bx::mtxIdentity(identity);
    bx::mtxOrtho(orthoView, 0.0f, _width, _height, 0.0f, -1.0f, 1.0f);
    if (viewNumber < 0) viewNumber = _viewNumber;

    bgfx::setViewTransform(viewNumber, orthoView, identity);
    bgfx::setViewRect(viewNumber, 0, 0, _width, _height);
}
RocketBgfxInterface::~RocketBgfxInterface() {
    if (_textureBuffers.size()) {
        for (const auto & buffer : _textureBuffers) {
            bgfx::destroyTexture(buffer.second);
        }
        _textureBuffers.clear();
    }

    for (auto item : _geometry) {
        bgfx::VertexBufferHandle vertexBuffer{ bgfx::invalidHandle };
        bgfx::IndexBufferHandle indexBuffer{ bgfx::invalidHandle };
        bgfx::TextureHandle texture{ bgfx::invalidHandle };

        std::tie(vertexBuffer, indexBuffer, texture) = item.second;

        bgfx::destroyVertexBuffer(vertexBuffer);
        bgfx::destroyIndexBuffer(indexBuffer);
    }
    _geometry.clear();

    bgfx::destroyDynamicVertexBuffer(_dynamicVertexBuffer);
    bgfx::destroyDynamicIndexBuffer(_dynamicIndexBuffer);
}
// Draw and then clear the dynamic geometry queue.
// @TODO: This is still multiple dynamic geomery draw calls. We need to actually check index/texture buffers and combine them if they match.
void RocketBgfxInterface::frame() {
    if (_batchGeometry.size() < 1)
        return;

    for (auto item : _batchGeometry) {
        const bgfx::Memory * vertexBuffer;
        const bgfx::Memory * indexBuffer;
        Rocket::Core::Vector2f translation;
        bgfx::TextureHandle texture{ bgfx::invalidHandle };
        bgfx::ProgramHandle _shader = _colorShader;

        std::tie(vertexBuffer, indexBuffer, texture, translation) = item;

        bgfx::updateDynamicVertexBuffer(_dynamicVertexBuffer, vertexBuffer);
        if (indexBuffer != nullptr) {
            bgfx::updateDynamicIndexBuffer(_dynamicIndexBuffer, indexBuffer);
        }
        
        if (bgfx::isValid(texture)) {
            bgfx::UniformHandle s_texture0 = bgfx::createUniform("s_texture0", bgfx::UniformType::Uniform1iv);
            bgfx::setTexture(0, s_texture0, texture);
            _shader = _textureShader;
        }

        float translationMatrix[16] = { 0 };
        bx::mtxTranslate(translationMatrix, translation.x, translation.y, 0.0f);
        bgfx::setTransform(translationMatrix);

        bgfx::setProgram(_shader);
        bgfx::setState(BGFX_STATE_RGB_WRITE
            | BGFX_STATE_ALPHA_WRITE
            | BGFX_STATE_MSAA
            | BGFX_STATE_BLEND_NORMAL);
        bgfx::submit(_viewNumber);

        
    }

    _batchGeometry.clear();
}
// This is dynamic geometry, so we use a dynamic buffer for this. Here, we queue the geometry calls into a vector storing its appropriate info
// At the frame call, we draw the entire collection and clear it.
void RocketBgfxInterface::RenderGeometry(Rocket::Core::Vertex* vertices, int num_vertices, int* indices, int num_indices, Rocket::Core::TextureHandle texture, const Rocket::Core::Vector2f& translation) {
    
    const bgfx::Memory * vertexBuffer = nullptr;
    const bgfx::Memory * indexBuffer = nullptr;
    bgfx::TextureHandle _texture{ bgfx::invalidHandle };

    if (num_indices) {
        indexBuffer = bgfx::copy(indices, num_indices * sizeof(int));
    }
    vertexBuffer = bgfx::copy(vertices, num_vertices * sizeof(Rocket::Core::Vertex));

    if (texture) {
       _texture = _textureBuffers[texture];
    }

    _batchGeometry.push_back(std::make_tuple(vertexBuffer, indexBuffer, _texture, translation));
}
// We store the compiled geometry internally as a tuple, and then reference it based on an int index handle we give back to rocket.
// Rocket provides INT32 indexes, RBGA8 textures and its vertex data defined in the header. we just compile them up into static buffers and go.
Rocket::Core::CompiledGeometryHandle RocketBgfxInterface::CompileGeometry(
    Rocket::Core::Vertex* vertices,
    int num_vertices,
    int* indices, int num_indices,
    Rocket::Core::TextureHandle texture)
{
    int returnIndex = _currentGeometryIndex;
    if (returnIndex < 1)
        returnIndex = 1;

    bgfx::VertexBufferHandle vertexBuffer{ bgfx::invalidHandle };
    bgfx::IndexBufferHandle indexBuffer{ bgfx::invalidHandle };
    bgfx::TextureHandle _texture{ bgfx::invalidHandle };

    if (num_indices) {
        indexBuffer = bgfx::createIndexBuffer(bgfx::copy(indices, num_indices * sizeof(int32_t)), BGFX_BUFFER_INDEX32);
    }

    vertexBuffer = bgfx::createVertexBuffer(
        bgfx::copy(vertices, num_vertices * sizeof(Rocket::Core::Vertex)),
        RocketVertexData::ms_decl);

    if (texture) {
        _texture = _textureBuffers[texture];
    }

    _geometry[returnIndex] = std::make_tuple(vertexBuffer, indexBuffer, _texture);

    _currentGeometryIndex = returnIndex + 1;
    return static_cast<Rocket::Core::CompiledGeometryHandle>(returnIndex);
}

void RocketBgfxInterface::RenderCompiledGeometry(
    Rocket::Core::CompiledGeometryHandle geometry,
    const Rocket::Core::Vector2f& translation)
{
    int requestedIndex = static_cast<int>(geometry);

    bgfx::VertexBufferHandle vertexBuffer;
    bgfx::IndexBufferHandle indexBuffer;
    bgfx::TextureHandle texture;

    bgfx::ProgramHandle shader = _colorShader;

    float translationMatrix[16] = { 0 };
    bx::mtxTranslate(translationMatrix, translation.x, translation.y, 0.0f);
    bgfx::setTransform(translationMatrix);

    std::tie(vertexBuffer, indexBuffer, texture) = _geometry[requestedIndex];
    if (bgfx::isValid(texture)) {
        bgfx::UniformHandle s_texture0 = bgfx::createUniform("s_texture0", bgfx::UniformType::Uniform1iv);
        bgfx::setTexture(0, s_texture0, texture);
        shader = _textureShader;
    }

    bgfx::setVertexBuffer(vertexBuffer);
    if (bgfx::isValid(indexBuffer)) {
        bgfx::setIndexBuffer(indexBuffer);
    }
    
    bgfx::setProgram(shader);
    bgfx::setState(BGFX_STATE_RGB_WRITE
        | BGFX_STATE_ALPHA_WRITE
        | BGFX_STATE_MSAA
        | BGFX_STATE_BLEND_NORMAL);
    bgfx::submit(_viewNumber);
}
void RocketBgfxInterface::ReleaseCompiledGeometry(Rocket::Core::CompiledGeometryHandle geometry) {
    bgfx::VertexBufferHandle vertexBuffer;
    bgfx::IndexBufferHandle indexBuffer;
    bgfx::TextureHandle texture;

    int requestedIndex = static_cast<int>(geometry);

    std::tie(vertexBuffer, indexBuffer, texture) = _geometry[requestedIndex];
    bgfx::destroyVertexBuffer(vertexBuffer);
    bgfx::destroyIndexBuffer(indexBuffer);

    _geometry.erase(requestedIndex);
}

// Rocket does a call to scissor enable and THEN calls the scissor region sizing. This is backwards to bgfx as we just call 0/value to scissor, but inline with openGL.
// So we have to internally track our scissor states of enabled/disabled and the values seperately, and re-set the bgfx scissor value every call.
void RocketBgfxInterface::EnableScissorRegion(bool enable) {
    _scissorParams.enabled = enable;

    if (_scissorParams.enabled && (_scissorParams.x > 0 || _scissorParams.y > 0 || _scissorParams.width > 0 || _scissorParams.height > 0)) {
        bgfx::setViewScissor(_viewNumber, _scissorParams.x, _scissorParams.y, _scissorParams.width, _scissorParams.height);
    }
    else {
        bgfx::setViewScissor(_viewNumber, 0, 0, 0, 0);
    }
    return;
}
void RocketBgfxInterface::SetScissorRegion(int x, int y, int w, int h) {
    _scissorParams = { _scissorParams.enabled,
        static_cast<uint16_t>(x),
        static_cast<uint16_t>(y),
        static_cast<uint16_t>(w) ,
        static_cast<uint16_t>(h) };


    if (_scissorParams.enabled) {
        bgfx::setViewScissor(_viewNumber, _scissorParams.x, _scissorParams.y, _scissorParams.width, _scissorParams.height);
    }
    else {
        bgfx::setViewScissor(_viewNumber, 0, 0, 0, 0);
    }
}
bool RocketBgfxInterface::LoadTexture(Rocket::Core::TextureHandle& texture_handle, Rocket::Core::Vector2i& texture_dimensions, const Rocket::Core::String& source) {
    // We load a texture from a file into a buffer, pass it to GenerateTexture
    // Load the image and get a pointer to the pixels in memory
    int w = 0, h = 0, channels = 0;
    unsigned char* ptr = stbi_load(source.CString(), &w, &h, &channels, STBI_rgb_alpha);

    // Throw error if the texture file didnt open and ready correctly
    if (!(ptr && w && h)) {
        throw std::runtime_error("RocketBgfxInterface::LoadTexture() - FAILED_TO_LOAD_IMAGE_FROM_FILE");
    }

    // Hand it off to the internal texture manager and then deallocate
    bool ret = GenerateTexture(texture_handle, ptr, Rocket::Core::Vector2i(w, h));
    free(ptr);

    return ret;
}
bool RocketBgfxInterface::GenerateTexture(Rocket::Core::TextureHandle& texture_handle, const Rocket::Core::byte* source, const Rocket::Core::Vector2i& source_dimensions) {
    int returnIndex = _currentTextureIndex;

    // Create the new texture handle
    const bgfx::Memory *sourceMemory = bgfx::copy(source, source_dimensions.x * source_dimensions.y * sizeof(uint32_t));
    //const bgfx::Memory *sourceMemory = bgfx::makeRef(source, source_dimensions.x * source_dimensions.y * sizeof(uint32_t));
    bgfx::TextureHandle texHandle = bgfx::createTexture2D(source_dimensions.x, source_dimensions.y,
        1, bgfx::TextureFormat::RGBA8,
        (BGFX_TEXTURE_U_CLAMP | BGFX_TEXTURE_V_CLAMP) | (BGFX_TEXTURE_MIN_POINT | BGFX_TEXTURE_MAG_POINT),
        sourceMemory);

    // Save the texture reference
    _textureBuffers[returnIndex] = texHandle;

    _currentTextureIndex = _currentTextureIndex + 1;
    texture_handle = returnIndex;

    return true;
}
void RocketBgfxInterface::ReleaseTexture(Rocket::Core::TextureHandle texture) {
    int requestedIndex = static_cast<int>(texture);

    if (_textureBuffers.find(requestedIndex) != _textureBuffers.end()) {
        bgfx::destroyTexture(_textureBuffers[requestedIndex]);  // clear the texture handle
        _textureBuffers.erase(requestedIndex);
    }

    return;
}