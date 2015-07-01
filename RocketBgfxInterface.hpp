#pragma once

#include <bgfx.h>
#include <memory>
#include <Rocket/Core/RenderInterface.h>

#include <unordered_map>

namespace space {
    namespace Render {
        class RocketBgfxInterface : 
            public Rocket::Core::RenderInterface 
        {
            struct RocketVertexData {
                float   m_position[2];
                uint8_t m_color[4];
                float   m_texCoords[2];

                static void init()
                {
                    ms_decl
                        .begin()
                        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
                        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
                        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
                        .end();
                };

                static bgfx::VertexDecl ms_decl;
            };
            
            /// The shader the render interface uses
            bgfx::ProgramHandle _colorShader, _textureShader;

            // Internal buffer state tracking
            int _viewNumber;
            int _currentGeometryIndex;
            int _currentTextureIndex;
            std::unordered_map<int, std::tuple<bgfx::VertexBufferHandle, bgfx::IndexBufferHandle, bgfx::TextureHandle>> _geometry;
            std::unordered_map<int, bgfx::TextureHandle>                  _textureBuffers;

            float _width;
            float _height;

            struct viewScissor_t {
                bool enabled;
                uint16_t x, y, width, height;
            };
            viewScissor_t _scissorParams;

            RocketBgfxInterface( const RocketBgfxInterface& ) = delete; // non construction-copyable
            RocketBgfxInterface& operator=( const RocketBgfxInterface& ) = delete; // non copyable
        public:
            RocketBgfxInterface(int viewId, float width, float height, bool configureView = true);
            RocketBgfxInterface(int viewId, bgfx::ProgramHandle colorShader, bgfx::ProgramHandle textureShader, float width, float height, bool configureView = true);
            
            virtual ~RocketBgfxInterface();
            
            /// Set the shader the RenderInterface is using.
            /// \param shader The shader you wish to use, null if nothing
            void setColorShader(bgfx::ProgramHandle shader) noexcept { _colorShader = shader; }
            void setTextureShader(bgfx::ProgramHandle shader) noexcept { _colorShader = shader; }
            bgfx::ProgramHandle getColorShader() const noexcept { return _colorShader; }
            bgfx::ProgramHandle getTextureShader() const noexcept { return _textureShader; }

            void setViewNumber(int viewNumber) noexcept { _viewNumber = viewNumber; }
            int getViewNumber() const noexcept { return _viewNumber; }

            void setViewParameters(int viewNumber = -1);

            // Rocket functions
            virtual void RenderGeometry(Rocket::Core::Vertex* vertices, int num_vertices, int* indices, int num_indices, Rocket::Core::TextureHandle texture, const Rocket::Core::Vector2f& translation) override;
            virtual Rocket::Core::CompiledGeometryHandle CompileGeometry(Rocket::Core::Vertex* vertices, int num_vertices, int* indices, int num_indices, Rocket::Core::TextureHandle texture) override;
            virtual void RenderCompiledGeometry(Rocket::Core::CompiledGeometryHandle geometry, const Rocket::Core::Vector2f& translation) override;
            virtual void ReleaseCompiledGeometry(Rocket::Core::CompiledGeometryHandle geometry) override;
            virtual void EnableScissorRegion(bool enable) override;
            virtual void SetScissorRegion(int x, int y, int width, int height) override;
            virtual bool LoadTexture(Rocket::Core::TextureHandle& texture_handle, Rocket::Core::Vector2i& texture_dimensions, const Rocket::Core::String& source) override;
            virtual bool GenerateTexture(Rocket::Core::TextureHandle& texture_handle, const Rocket::Core::byte* source, const Rocket::Core::Vector2i& source_dimensions) override;
            virtual void ReleaseTexture(Rocket::Core::TextureHandle texture) override;
        };
    }
}