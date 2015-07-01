#include "Render/RocketBgfxInterface.hpp"
#include "Render/RocketSystemInterface.hpp"

#include "vs_BgfxRocketRenderTest.bin.h"
#include "fs_BgfxRocketRenderTestColor.bin.h"
#include "fs_BgfxRocketRenderTestTexture.bin.h"

#include "common.h"
#include "bgfx_utils.h"
#include "camera.h"

#include <iostream>

#define ROCKET_STATIC_LIB
#include <Rocket/Core.h>
#include <Rocket/Debugger.h>
#include <Rocket/Controls.h>

#include "Logging.hpp"
Rocket::Core::Context* rocketContext = NULL;

bool leftDown = false;
bool rightDown = false;

void injectRocketButtons(Rocket::Core::Context*, entry::MouseState mouseState) {
    rocketContext->ProcessMouseMove(mouseState.m_mx, mouseState.m_my, 0);
    if (mouseState.m_buttons[entry::MouseButton::Left] && !leftDown) {
        rocketContext->ProcessMouseButtonDown(0, 0);
        leftDown = true;
    } else if (!mouseState.m_buttons[entry::MouseButton::Left] && leftDown) {
        rocketContext->ProcessMouseButtonUp(0, 0);
        leftDown = false;
    }

    if (mouseState.m_buttons[entry::MouseButton::Right] && !rightDown) {
        rocketContext->ProcessMouseButtonDown(1, 0);
        rightDown = true;
    }
    else if (!mouseState.m_buttons[entry::MouseButton::Right] && rightDown) {
        rocketContext->ProcessMouseButtonUp(1, 0);
        rightDown = false;
    }

    return;
}
using namespace space::Render;

int _main_(int /*_argc*/, char** /*_argv*/) {
    uint32_t width = 1280;
	uint32_t height = 720;
	uint32_t debug = BGFX_DEBUG_TEXT;
	uint32_t reset = BGFX_RESET_MSAA_X16;

	bgfx::init(bgfx::RendererType::Direct3D11);
	bgfx::reset(width, height, reset);

	// Enable debug text.
	bgfx::setDebug(debug);

	// Set view 0 clear state.
	bgfx::setViewClear(0
		, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH
		, 0xffffffff
		, 1.0f
		, 0
		);
    
    // Load the shaders
    const bgfx::Memory* vs_BgfxRocketRender;
    const bgfx::Memory* fs_BgfxRocketRenderColor;
    const bgfx::Memory* fs_BgfxRocketRenderTexture;

    switch (bgfx::getRendererType())
    {
    case bgfx::RendererType::Direct3D9:
        vs_BgfxRocketRender = bgfx::makeRef(vs_BgfxRocketRenderTest_dx9, sizeof(vs_BgfxRocketRenderTest_dx9));
        fs_BgfxRocketRenderColor = bgfx::makeRef(fs_BgfxRocketRenderTestColor_dx9, sizeof(fs_BgfxRocketRenderTestColor_dx9));
        fs_BgfxRocketRenderTexture = bgfx::makeRef(fs_BgfxRocketRenderTestTexture_dx9, sizeof(fs_BgfxRocketRenderTestTexture_dx9));
        break;

    case bgfx::RendererType::Direct3D11:
    case bgfx::RendererType::Direct3D12:
        vs_BgfxRocketRender = bgfx::makeRef(vs_BgfxRocketRenderTest_dx11, sizeof(vs_BgfxRocketRenderTest_dx11));
        fs_BgfxRocketRenderColor = bgfx::makeRef(fs_BgfxRocketRenderTestColor_dx11, sizeof(fs_BgfxRocketRenderTestColor_dx11));
        fs_BgfxRocketRenderTexture = bgfx::makeRef(fs_BgfxRocketRenderTestTexture_dx11, sizeof(fs_BgfxRocketRenderTestTexture_dx11));
        break;

    default:
        vs_BgfxRocketRender = bgfx::makeRef(vs_BgfxRocketRenderTest_glsl, sizeof(vs_BgfxRocketRenderTest_glsl));
        fs_BgfxRocketRenderColor = bgfx::makeRef(fs_BgfxRocketRenderTestColor_glsl, sizeof(fs_BgfxRocketRenderTestColor_glsl));
        fs_BgfxRocketRenderTexture = bgfx::makeRef(fs_BgfxRocketRenderTestTexture_glsl, sizeof(fs_BgfxRocketRenderTestTexture_glsl));
        break;
    }
    bgfx::ShaderHandle vsh = bgfx::createShader(vs_BgfxRocketRender);
	bgfx::ShaderHandle fsh_color = bgfx::createShader(fs_BgfxRocketRenderColor);
    bgfx::ShaderHandle fsh_texture = bgfx::createShader(fs_BgfxRocketRenderTexture);
	
    // Create program from shaders.
	bgfx::ProgramHandle rocketColorProgram = bgfx::createProgram(vsh, fsh_color, true);
    bgfx::ProgramHandle rocketTextureProgram = bgfx::createProgram(vsh, fsh_texture, true);

    // Setup UI View
    
    bgfx::setViewClear(1
        , BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH
        , 0x3070F0FF
        );

    float orthoView[16] = { 0 };
    float identity[16] = { 0 };
    bx::mtxIdentity(identity);
    bx::mtxOrtho(orthoView, 0.0f, width, height, 0.0f, -1.0f, 1.0f);

    float proj[16];
    bx::mtxProj(proj, 60.0f, float(width) / float(height), 0.1f, 100.0f);

    // initialize the rocket interface
    RocketBgfxInterface * bgfxRocket = new RocketBgfxInterface(1, rocketColorProgram, rocketTextureProgram, width, height);
    RocketSystemInterface * rocketSystem = new RocketSystemInterface();
    
    bgfxRocket->setViewParameters();

    Rocket::Core::SetRenderInterface(bgfxRocket);
    Rocket::Core::SetSystemInterface(rocketSystem);
    
    Rocket::Core::Initialise();
    Rocket::Controls::Initialise();
    Rocket::Core::Log::Initialise();
    
    rocketContext = Rocket::Core::CreateContext("main", Rocket::Core::Vector2i(width, height));
    Rocket::Debugger::Initialise(rocketContext);
    
    Rocket::Core::String font_names[4];
    font_names[0] = "Delicious-Roman.otf";
    font_names[1] = "Delicious-Italic.otf";
    font_names[2] = "Delicious-Bold.otf";
    font_names[3] = "Delicious-BoldItalic.otf";

    for (int i = 0; i < sizeof(font_names) / sizeof(Rocket::Core::String); i++)
    {
        if (Rocket::Core::FontDatabase::LoadFontFace(Rocket::Core::String("assets/") + font_names[i])) {
            std::cout << font_names[0].CString() << " loaded\n";
        }
    }

    Rocket::Core::ElementDocument* document = rocketContext->LoadDocument("data/test.rml");
    if (document != NULL) {
        document->Show();
    }

    // bgfx sample framework code
    int64_t timeOffset = bx::getHPCounter();
    entry::MouseState mouseState;
    
    cameraCreate();
    float initialPos[3] = { 0.0f, 18.0f, -40.0f };
	cameraSetPosition(initialPos);
	cameraSetVerticalAngle(-0.35f);

    while (!entry::processEvents(width, height, debug, reset, &mouseState) )
	{
		int64_t now = bx::getHPCounter();
		static int64_t last = now;
		const int64_t frameTime = now - last;
		last = now;
		const double freq = double(bx::getHPFrequency() );
		const double toMs = 1000.0/freq;

		float time = (float)( (now-timeOffset)/double(bx::getHPFrequency() ) );
		const float deltaTime = float(frameTime/freq);

		// Use debug font to print information about this example.
		bgfx::dbgTextClear();
        
		bgfx::dbgTextPrintf(0, 1, 0x4f, "Renderer: %s", 
            bgfx::getRendererName(bgfx::getRendererType()));
		bgfx::dbgTextPrintf(0, 2, 0x0f, "Frame: %.2f[ms], %.2f[fps]", 
            double(frameTime)*toMs, 
            1000.0f/(double(frameTime)*toMs));

        float at[3] = { 0.0f, 0.0f,   0.0f };
        float eye[3] = { 0.0f, 0.0f, -35.0f };
        float view[16];

        bx::mtxLookAt(view, eye, at);

 
        bgfx::setViewTransform(0, view, proj);
        bgfx::setViewRect(0, 0, 0, width, height);
        
        bgfx::submit(0);

        injectRocketButtons(rocketContext, mouseState);
        rocketContext->Update();
        rocketContext->Render();

		bgfx::frame();
	}

    Rocket::Core::Shutdown();
    
	bgfx::destroyProgram(rocketColorProgram);
    bgfx::destroyProgram(rocketTextureProgram);
	bgfx::shutdown();

	return 0;
}