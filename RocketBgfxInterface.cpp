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
}
RocketBgfxInterface::RocketBgfxInterface(int viewNumber, bgfx::ProgramHandle colorShader, bgfx::ProgramHandle textureShader, float _width, float _height, bool configureView)
    : _colorShader(colorShader), _textureShader(textureShader), _width(_width), _height(_height), _currentTextureIndex(1), _viewNumber(viewNumber), _scissorParams({ false, 0, 0, 0, 0 })
{
    RocketVertexData::init();
    if (configureView) setViewParameters();
}

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



}

void RocketBgfxInterface::RenderGeometry(Rocket::Core::Vertex* vertices, int num_vertices, int* indices, int num_indices, Rocket::Core::TextureHandle texture, const Rocket::Core::Vector2f& translation) {
    bgfx::VertexBufferHandle vertexBuffer;
    bgfx::IndexBufferHandle indexBuffer;

    bgfx::ProgramHandle _shader = _colorShader;

    if (num_indices) {
        indexBuffer = bgfx::createIndexBuffer(bgfx::makeRef(indices, num_indices * sizeof(float)), BGFX_BUFFER_INDEX32);
    }

    vertexBuffer = bgfx::createVertexBuffer(
        bgfx::makeRef(vertices, num_vertices * sizeof(Rocket::Core::Vertex)),
        RocketVertexData::ms_decl);

    if (texture) {
        bgfx::UniformHandle s_texture0 = bgfx::createUniform("s_texture0", bgfx::UniformType::Uniform1iv);
        bgfx::setTexture(0, s_texture0, _textureBuffers[texture]);
        _shader = _textureShader;
    }

    float translationMatrix[16] = { 0 };
    bx::mtxTranslate(translationMatrix, translation.x, translation.y, 0.0f);
    bgfx::setTransform(translationMatrix);

    bgfx::setVertexBuffer(vertexBuffer);
    if (num_indices) {
        bgfx::setIndexBuffer(indexBuffer);
    }

    bgfx::setProgram(_shader);
    bgfx::setState(BGFX_STATE_RGB_WRITE
        | BGFX_STATE_ALPHA_WRITE
        | BGFX_STATE_MSAA
        | BGFX_STATE_BLEND_NORMAL);
    bgfx::submit(_viewNumber);

    bgfx::destroyVertexBuffer(vertexBuffer);
    bgfx::destroyIndexBuffer(indexBuffer);
}

Rocket::Core::CompiledGeometryHandle RocketBgfxInterface::CompileGeometry(
    Rocket::Core::Vertex* vertices,
    int num_vertices,
    int* indices, int num_indices, 
    Rocket::Core::TextureHandle texture) 
{

    return 0;
    int returnIndex = _currentGeometryIndex;

    bgfx::VertexBufferHandle vertexBuffer;
    bgfx::IndexBufferHandle indexBuffer;

    if (num_indices) {
        indexBuffer = bgfx::createIndexBuffer(bgfx::makeRef(indices, num_indices * sizeof(float)));
    }

    vertexBuffer = bgfx::createVertexBuffer(
        bgfx::makeRef(vertices, num_vertices * sizeof(Rocket::Core::Vertex)),
        RocketVertexData::ms_decl);

    if (texture) {
        _geometry[returnIndex] = std::make_tuple(vertexBuffer, indexBuffer, _textureBuffers[texture]);
    } else {
        _geometry[returnIndex] = std::make_tuple(vertexBuffer, indexBuffer, bgfx::TextureHandle());
    }

    _currentGeometryIndex = _currentGeometryIndex + 1;
    return returnIndex;
}
void RocketBgfxInterface::RenderCompiledGeometry(
    Rocket::Core::CompiledGeometryHandle geometry,
    const Rocket::Core::Vector2f& translation)
{
    int requestedIndex = static_cast<int>(geometry);

    bgfx::VertexBufferHandle vertexBuffer;
    bgfx::IndexBufferHandle indexBuffer;
    bgfx::TextureHandle texture;
    bool hasTexture = false;
    bgfx::ProgramHandle _shader = _colorShader;

    float translationMatrix[16] = { 0 };
    bx::mtxTranslate(translationMatrix, translation.x, translation.y, 0.0f);
    bgfx::setTransform(translationMatrix);

    std::tie(vertexBuffer, indexBuffer, texture) = _geometry[requestedIndex];
    bgfx::setVertexBuffer(vertexBuffer);
    if (bgfx::isValid(indexBuffer)) {
        bgfx::setIndexBuffer(indexBuffer);
    }

    if (bgfx::isValid(texture)) {
        bgfx::UniformHandle s_texture0 = bgfx::createUniform("s_texture0", bgfx::UniformType::Uniform1iv);
        bgfx::setTexture(0, s_texture0, _textureBuffers[requestedIndex]);
        _shader = _textureShader;
    }
    bgfx::setProgram(_shader);
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
void RocketBgfxInterface::EnableScissorRegion(bool enable) {
    _scissorParams.enabled = enable;

    if (_scissorParams.enabled && (_scissorParams.x > 0 || _scissorParams.y > 0 || _scissorParams.width > 0 || _scissorParams.height > 0) ) {
        bgfx::setViewScissor(_viewNumber, _scissorParams.x, _scissorParams.y, _scissorParams.width, _scissorParams.height);
    } else {
        bgfx::setViewScissor(_viewNumber, 0, 0, 0, 0);
    }
    return;
}
void RocketBgfxInterface::SetScissorRegion(int x, int y, int w, int h) {
    _scissorParams = {   _scissorParams.enabled,
                        static_cast<uint16_t>(x), 
                        static_cast<uint16_t>(y),
                        static_cast<uint16_t>(w) ,
                        static_cast<uint16_t>(h) };

    if (_scissorParams.enabled) {
        bgfx::setViewScissor(_viewNumber, _scissorParams.x, _scissorParams.y, _scissorParams.width, _scissorParams.height);
    } else {
        bgfx::setViewScissor(_viewNumber, 0, 0, 0, 0);
    }
}
bool RocketBgfxInterface::LoadTexture(Rocket::Core::TextureHandle& texture_handle, Rocket::Core::Vector2i& texture_dimensions, const Rocket::Core::String& source) {
    // We load a texture from a file into a buffer, pass it to GenerateTexture
    // Load the image and get a pointer to the pixels in memory
    int w = 0, h = 0, channels = 0;
    unsigned char* ptr = stbi_load(source.CString(), &w, &h, &channels, STBI_rgb_alpha);
    
    if (!(ptr && w && h)) {
        throw std::runtime_error("RocketBgfxInterface::LoadTexture() - FAILED_TO_LOAD_IMAGE_FROM_FILE");
    }

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