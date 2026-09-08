TARGET		:= dhewm3
TITLE		:= DHEWM3ES2

LIBS = -lopenal -lpthread -lvitaGL -lvitashark -lmathneon -lvorbisfile -lvorbis -logg  -lspeexdsp -lmpg123 \
	-lSDL2 -lc -lSceCommonDialog_stub -lSceAudio_stub -lSceLibKernel_stub -lSceShaccCg_stub -lz \
	-lSceShaccCgExt -ltaihen_stub -lpng -lSceDisplay_stub -lSceGxm_stub -lSceFios2_stub \
	-lSceCtrl_stub -lSceTouch_stub -lSceMotion_stub -lm -lSceAppMgr_stub \
	-lSceAppUtil_stub -lScePgf_stub -ljpeg -lSceRtc_stub -lScePower_stub -lcurl -lssl -lcrypto \
	-lSceAudioIn_stub -lSceKernelDmacMgr_stub -lSceIme_stub -lSceHid_stub -lSceIofilemgr_stub \
	-Wl,--whole-archive -lSceNet_stub -lSceSysmodule_stub -lSceNetCtl_stub -Wl,--no-whole-archive

MISCOBJS = \
	cm/CollisionModel_contacts.o \
    cm/CollisionModel_contents.o \
    cm/CollisionModel_debug.o \
    cm/CollisionModel_files.o \
    cm/CollisionModel_load.o \
    cm/CollisionModel_rotate.o \
    cm/CollisionModel_trace.o \
    cm/CollisionModel_translate.o \
	tools/compilers/aas/AASBuild.o \
    tools/compilers/aas/AASBuild_file.o \
    tools/compilers/aas/AASBuild_gravity.o \
    tools/compilers/aas/AASBuild_ledge.o \
    tools/compilers/aas/AASBuild_merge.o \
    tools/compilers/aas/AASCluster.o \
    tools/compilers/aas/AASFile.o \
    tools/compilers/aas/AASFile_optimize.o \
    tools/compilers/aas/AASFile_sample.o \
    tools/compilers/aas/AASReach.o \
    tools/compilers/aas/AASFileManager.o \
    tools/compilers/aas/Brush.o \
    tools/compilers/aas/BrushBSP.o \
	sound/snd_cache.o \
    sound/snd_decoder.o \
    sound/snd_efxfile.o \
    sound/snd_emitter.o \
    sound/snd_shader.o \
    sound/snd_system.o \
    sound/snd_wavefile.o \
    sound/snd_world.o

UIOBJS = \
	ui/BindWindow.o \
    ui/ChoiceWindow.o \
    ui/DeviceContext.o \
    ui/EditWindow.o \
    ui/FieldWindow.o \
    ui/GameBearShootWindow.o \
    ui/GameBustOutWindow.o \
    ui/GameSSDWindow.o \
    ui/GuiScript.o \
    ui/ListGUI.o \
    ui/ListWindow.o \
    ui/MarkerWindow.o \
    ui/RegExp.o \
    ui/RenderWindow.o \
    ui/SimpleWindow.o \
    ui/SliderWindow.o \
    ui/UserInterface.o \
    ui/Window.o \
    ui/Winvar.o

IDLIBOBJS = \
	idlib/bv/Bounds.o \
    idlib/bv/Frustum.o \
    idlib/bv/Sphere.o \
    idlib/bv/Box.o \
    idlib/geometry/DrawVert.o \
    idlib/geometry/Winding2D.o \
    idlib/geometry/Surface_SweptSpline.o \
    idlib/geometry/Winding.o \
    idlib/geometry/Surface.o \
    idlib/geometry/Surface_Patch.o \
    idlib/geometry/TraceModel.o \
    idlib/geometry/JointTransform.o \
    idlib/hashing/CRC32.o \
    idlib/hashing/MD4.o \
    idlib/hashing/MD5.o \
    idlib/math/Angles.o \
    idlib/math/Lcp.o \
    idlib/math/Math.o \
    idlib/math/Matrix.o \
    idlib/math/Ode.o \
    idlib/math/Plane.o \
    idlib/math/Pluecker.o \
    idlib/math/Polynomial.o \
    idlib/math/Quat.o \
    idlib/math/Rotation.o \
    idlib/math/Simd.o \
    idlib/math/Simd_Generic.o \
    idlib/math/Simd_MMX.o \
    idlib/math/Simd_SSE.o \
    idlib/math/Simd_SSE2.o \
    idlib/math/Simd_SSE3.o \
    idlib/math/Simd_NEON.o \
    idlib/math/Vector.o \
    idlib/BitMsg.o \
    idlib/LangDict.o \
    idlib/Lexer.o \
    idlib/Lib.o \
    idlib/containers/HashIndex.o \
    idlib/Dict.o \
    idlib/Str.o \
    idlib/Parser.o \
    idlib/MapFile.o \
    idlib/CmdArgs.o \
    idlib/Token.o \
    idlib/Base64.o \
    idlib/Timer.o \
    idlib/Heap.o

RENDEREROBJS = \
    renderer/glsl/cubeMapShaderFP.o \
    renderer/glsl/diffuseMapShaderVP.o \
    renderer/glsl/diffuseCubeShaderVP.o \
    renderer/glsl/diffuseMapShaderFP.o \
    renderer/glsl/fogShaderFP.o \
    renderer/glsl/fogShaderVP.o \
    renderer/glsl/blendLightShaderVP.o \
    renderer/glsl/interactionPhongShaderFP.o \
    renderer/glsl/interactionPhongShaderVP.o \
    renderer/glsl/interactionShaderFP.o \
    renderer/glsl/interactionShaderVP.o \
    renderer/glsl/reflectionCubeShaderVP.o \
    renderer/glsl/skyboxCubeShaderVP.o \
    renderer/glsl/stencilShadowShaderFP.o \
    renderer/glsl/stencilShadowShaderVP.o \
    renderer/glsl/zfillClipShaderFP.o \
    renderer/glsl/zfillClipShaderVP.o \
    renderer/glsl/zfillShaderFP.o \
    renderer/glsl/zfillShaderVP.o \
	renderer/Cinematic.o \
    renderer/GuiModel.o \
    renderer/Image_files.o \
    renderer/Image_init.o \
    renderer/Image_load.o \
    renderer/Image_process.o \
    renderer/Image_program.o \
    renderer/Interaction.o \
    renderer/Material.o \
    renderer/MegaTexture.o \
    renderer/Model.o \
    renderer/ModelDecal.o \
    renderer/ModelManager.o \
    renderer/ModelOverlay.o \
    renderer/Model_beam.o \
    renderer/Model_ase.o \
    renderer/Model_liquid.o \
    renderer/Model_lwo.o \
    renderer/Model_ma.o \
    renderer/Model_md3.o \
    renderer/Model_md5.o \
    renderer/Model_prt.o \
    renderer/Model_sprite.o \
    renderer/RenderEntity.o \
    renderer/RenderSystem.o \
    renderer/RenderSystem_init.o \
    renderer/RenderWorld.o \
    renderer/RenderWorld_demo.o \
    renderer/RenderWorld_load.o \
    renderer/RenderWorld_portals.o \
    renderer/VertexCache.o \
    renderer/draw_gles2.o \
    renderer/draw_common.o \
    renderer/tr_backend.o \
    renderer/tr_deform.o \
    renderer/tr_font.o \
    renderer/tr_guisurf.o \
    renderer/tr_light.o \
    renderer/tr_lightrun.o \
    renderer/tr_main.o \
    renderer/tr_orderIndexes.o \
    renderer/tr_polytope.o \
    renderer/tr_render.o \
    renderer/tr_shadowbounds.o \
    renderer/tr_stencilshadow.o \
    renderer/tr_subview.o \
    renderer/tr_trace.o \
    renderer/tr_trisurf.o \
    renderer/tr_turboshadow.o \
    renderer/etc_android.o \
    renderer/framebuffer.o

SYSOBJS = \
	sys/cpu.o \
    sys/threads.o \
    sys/events.o \
    sys/sys_local.o \
    sys/posix/posix_net.o \
    sys/posix/posix_main.o \
    sys/linux/main.o \
    sys/vita_alloc.o \
	sys/glimp.o
	
FRAMEWORKOBJS = \
	framework/CVarSystem.o \
    framework/CmdSystem.o \
    framework/Common.o \
    framework/Compressor.o \
    framework/Console.o \
    framework/DemoFile.o \
    framework/DeclAF.o \
    framework/DeclEntityDef.o \
    framework/DeclFX.o \
    framework/DeclManager.o \
    framework/DeclParticle.o \
    framework/DeclPDA.o \
    framework/DeclSkin.o \
    framework/DeclTable.o \
    framework/EditField.o \
    framework/EventLoop.o \
    framework/File.o \
    framework/FileSystem.o \
    framework/KeyInput.o \
    framework/UsercmdGen.o \
    framework/Session_menu.o \
    framework/Session.o \
    framework/PreparedCachePack.o \
    framework/async/AsyncClient.o \
    framework/async/AsyncNetwork.o \
    framework/async/AsyncServer.o \
    framework/async/MsgChannel.o \
    framework/async/NetworkSystem.o \
    framework/async/ServerScan.o \
	framework/minizip/unzip.o \
	framework/minizip/ioapi.o

GAMEOBJS = \
	game/AF.o \
    game/AFEntity.o \
    game/Actor.o \
    game/Camera.o \
    game/Entity.o \
    game/BrittleFracture.o \
    game/Fx.o \
    game/GameEdit.o \
    game/Game_local.o \
    game/Game_network.o \
    game/Item.o \
    game/IK.o \
    game/Light.o \
    game/Misc.o \
    game/Mover.o \
    game/Moveable.o \
    game/MultiplayerGame.o \
    game/Player.o \
    game/PlayerIcon.o \
    game/PlayerView.o \
    game/Projectile.o \
    game/Pvs.o \
    game/SecurityCamera.o \
    game/SmokeParticles.o \
    game/Sound.o \
    game/Target.o \
    game/Trigger.o \
    game/Weapon.o \
    game/WorldSpawn.o \
    game/ai/AAS.o \
    game/ai/AAS_debug.o \
    game/ai/AAS_pathing.o \
    game/ai/AAS_routing.o \
    game/ai/AI.o \
    game/ai/AI_events.o \
    game/ai/AI_pathing.o \
    game/ai/AI_Vagary.o \
    game/gamesys/DebugGraph.o \
    game/gamesys/Class.o \
    game/gamesys/Event.o \
    game/gamesys/SaveGame.o \
    game/gamesys/SysCmds.o \
    game/gamesys/SysCvar.o \
    game/gamesys/TypeInfo.o \
    game/anim/Anim.o \
    game/anim/Anim_Blend.o \
    game/anim/Anim_Import.o \
    game/anim/Anim_Testmodel.o \
    game/script/Script_Compiler.o \
    game/script/Script_Interpreter.o \
    game/script/Script_Program.o \
    game/script/Script_Thread.o \
    game/physics/Clip.o \
    game/physics/Force.o \
    game/physics/Force_Constant.o \
    game/physics/Force_Drag.o \
    game/physics/Force_Field.o \
    game/physics/Force_Spring.o \
    game/physics/Physics.o \
    game/physics/Physics_AF.o \
    game/physics/Physics_Actor.o \
    game/physics/Physics_Base.o \
    game/physics/Physics_Monster.o \
    game/physics/Physics_Parametric.o \
    game/physics/Physics_Player.o \
    game/physics/Physics_RigidBody.o \
    game/physics/Physics_Static.o \
    game/physics/Physics_StaticMulti.o \
    game/physics/Push.o

OBJS     := $(GAMEOBJS) $(FRAMEWORKOBJS) $(SYSOBJS) $(RENDEREROBJS) $(MISCOBJS) $(UIOBJS) $(IDLIBOBJS)

PREFIX  = arm-vita-eabi
CC      = $(PREFIX)-gcc
CXX      = $(PREFIX)-g++
CFLAGS  = -fsigned-char -Wl,-q,--wrap,fopen,--wrap,opendir,--wrap,open,--wrap,access,--wrap,malloc,--wrap,memalign,--wrap,free,--wrap,calloc,--wrap,realloc \
	-O3 -flto -g -fno-optimize-sibling-calls -DNODLL -DNOEFX -D__unix__ -DSDL_DISABLE_XMMINTRIN_H \
	-ffast-math -mtune=cortex-a9 -mfpu=neon -DVITA -I. -Igame -D__SSE__  -D__SSE2__ -D__SSE3__ \
	-I$(VITASDK)/$(PREFIX)/include/SDL2 -DSDL_DISABLE_EMMINTRIN_H -DSDL_DISABLE_PMMINTRIN_H 
	#-DNO_LIGHT
CXXFLAGS  = $(CFLAGS) -std=gnu++11 -fpermissive
ASFLAGS = $(CFLAGS)

release:
	make clean
	make -f Makefile2 -j14
	make clean
	make all -j14

all: $(TARGET).vpk

$(TARGET).vpk: $(TARGET).velf
	vita-make-fself -c -s $< build/eboot.bin
	vita-mksfoex -s TITLE_ID=$(TITLE) -d ATTRIBUTE2=12 "Doom 3" param.sfo
	vita-pack-vpk -s param.sfo -b build/eboot.bin \
		--add livearea/icon0.png=sce_sys/icon0.png \
		--add livearea/bg.png=sce_sys/livearea/contents/bg.png \
		--add livearea/startup.png=sce_sys/livearea/contents/startup.png \
		--add livearea/template.xml=sce_sys/livearea/contents/template.xml \
		--add build/no_lights.bin=no_lights.bin \
		$(TARGET).vpk

%.velf: %.elf
	cp $< $<.unstripped.elf
	$(PREFIX)-strip -g $<
	vita-elf-create $< $@

$(TARGET).elf: $(OBJS)
	$(CXX) $(CXXFLAGS) $^ $(LIBS) -o $@

clean:
	@rm -rf $(TARGET).velf $(TARGET).elf $(OBJS) $(TARGET).elf.unstripped.elf $(TARGET).vpk build/eboot.bin ./param.sfo
