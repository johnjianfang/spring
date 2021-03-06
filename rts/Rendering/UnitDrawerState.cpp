/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "UnitDrawerState.hpp"
#include "UnitDrawer.h"
#include "Game/Camera.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/ShadowHandler.h"
#include "Rendering/Env/ISky.h"
#include "Rendering/Env/IWater.h"
#include "Rendering/Env/CubeMapHandler.h"
#include "Rendering/Env/SkyLight.h"
#include "Rendering/GL/myGL.h"
#include "Rendering/Shaders/ShaderHandler.h"
#include "Rendering/Shaders/Shader.h"
#include "Sim/Misc/TeamHandler.h"
#include "System/Config/ConfigHandler.h"



IUnitDrawerState* IUnitDrawerState::GetInstance(bool haveARB, bool haveGLSL) {
	IUnitDrawerState* instance = NULL;

	if (!haveARB && !haveGLSL) {
		instance = new UnitDrawerStateFFP();
	} else {
		if (!haveGLSL) {
			instance = new UnitDrawerStateARB();
		} else {
			instance = new UnitDrawerStateGLSL();
		}
	}

	return instance;
}


void IUnitDrawerState::EnableCommon(const CUnitDrawer* ud) {
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMultMatrixf(camera->GetViewMatrix());
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	modelShaders[MODEL_SHADER_ACTIVE] = (shadowHandler->shadowsLoaded)?
		modelShaders[MODEL_SHADER_SHADOW]:
		modelShaders[MODEL_SHADER_BASIC ];
	modelShaders[MODEL_SHADER_ACTIVE]->Enable();

	// TODO: refactor to use EnableTexturesCommon
	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_2D);

	glActiveTexture(GL_TEXTURE1);
	glEnable(GL_TEXTURE_2D);

	if (shadowHandler->shadowsLoaded) {
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, shadowHandler->shadowTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB, GL_COMPARE_R_TO_TEXTURE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC_ARB, GL_LEQUAL);
		glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE_ARB, GL_LUMINANCE);
		glEnable(GL_TEXTURE_2D);
	}

	glActiveTexture(GL_TEXTURE3);
	glEnable(GL_TEXTURE_CUBE_MAP_ARB);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, cubeMapHandler->GetEnvReflectionTextureID());

	glActiveTexture(GL_TEXTURE4);
	glEnable(GL_TEXTURE_CUBE_MAP_ARB);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, cubeMapHandler->GetSpecularTextureID());

	glActiveTexture(GL_TEXTURE0);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void IUnitDrawerState::DisableCommon(const CUnitDrawer* ud) {
	// TODO: refactor to use DisableTexturesCommon
	glActiveTexture(GL_TEXTURE1);
	glDisable(GL_TEXTURE_2D);

	glActiveTexture(GL_TEXTURE2);
	glDisable(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB, GL_NONE);
	glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE_ARB, GL_LUMINANCE);

	glActiveTexture(GL_TEXTURE3);
	glDisable(GL_TEXTURE_CUBE_MAP_ARB);

	glActiveTexture(GL_TEXTURE4);
	glDisable(GL_TEXTURE_CUBE_MAP_ARB);

	glActiveTexture(GL_TEXTURE0);

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}


void IUnitDrawerState::EnableTexturesCommon(const CUnitDrawer* ud) {
	glEnable(GL_TEXTURE_2D);

	glActiveTexture(GL_TEXTURE1);
	glEnable(GL_TEXTURE_2D);

	if (shadowHandler->shadowsLoaded) {
		glActiveTexture(GL_TEXTURE2);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB, GL_COMPARE_R_TO_TEXTURE);
		glEnable(GL_TEXTURE_2D);
	}

	glActiveTexture(GL_TEXTURE3);
	glEnable(GL_TEXTURE_CUBE_MAP_ARB);
	glActiveTexture(GL_TEXTURE4);
	glEnable(GL_TEXTURE_CUBE_MAP_ARB);

	glActiveTexture(GL_TEXTURE0);
}

void IUnitDrawerState::DisableTexturesCommon(const CUnitDrawer* ud) {
	glActiveTexture(GL_TEXTURE1);
	glDisable(GL_TEXTURE_2D);

	glActiveTexture(GL_TEXTURE2);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB, GL_NONE);

	glDisable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE3);

	glDisable(GL_TEXTURE_CUBE_MAP_ARB);
	glActiveTexture(GL_TEXTURE4);
	glDisable(GL_TEXTURE_CUBE_MAP_ARB);
	glActiveTexture(GL_TEXTURE0);

	glDisable(GL_TEXTURE_2D);
}



bool UnitDrawerStateFFP::CanEnable(const CUnitDrawer* ud) const {
	// ARB standard does not seem to support vertex program + clipplanes
	// (used for reflective passes) at once ==> not true but needs option
	// ARB_position_invariant ==> the ARB shaders already have this, test
	// if RHS can be removed
	//
	// old UnitDrawer comments:
	//   GL_VERTEX_PROGRAM_ARB is very slow on ATIs for some reason
	//   if clip planes are enabled, check later after driver updates
	//
	//   ATI has issues with textures, clip planes and shader programs
	//   at once - very low performance
	return (!ud->advShading || water->IsDrawReflection());
}

void UnitDrawerStateFFP::Enable(const CUnitDrawer* ud) {
	glEnable(GL_LIGHTING);
	glLightfv(GL_LIGHT1, GL_POSITION, sky->GetLight()->GetLightDir());
	glEnable(GL_LIGHT1);

	CUnitDrawer::SetupBasicS3OTexture1();
	CUnitDrawer::SetupBasicS3OTexture0();

	// Set material color
	static const float cols[] = {1.0f, 1.0f, 1.0, 1.0f};

	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, cols);
	glColor4fv(cols);
}

void UnitDrawerStateFFP::Disable(const CUnitDrawer* ud) {
	glDisable(GL_LIGHTING);
	glDisable(GL_LIGHT1);

	CUnitDrawer::CleanupBasicS3OTexture1();
	CUnitDrawer::CleanupBasicS3OTexture0();
}


void UnitDrawerStateFFP::EnableTextures(const CUnitDrawer*) {
	glEnable(GL_LIGHTING);
	glColor3f(1.0f, 1.0f, 1.0f);
	glEnable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE1);
	glEnable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE0);
}

void UnitDrawerStateFFP::DisableTextures(const CUnitDrawer*) {
	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE1);
	glDisable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE0);
	glDisable(GL_TEXTURE_2D);
}

void UnitDrawerStateFFP::SetTeamColor(int team, float alpha) const {
	// non-shader case via texture combiners
	CUnitDrawer::SetBasicTeamColour(team, alpha);
}




bool UnitDrawerStateARB::Init(const CUnitDrawer* ud) {
	modelShaders.resize(MODEL_SHADER_LAST, NULL);

	if (!globalRendering->haveARB) {
		// not possible to do (ARB) shader-based model rendering
		return false;
	}
	if (!configHandler->GetBool("AdvUnitShading")) {
		// not allowed to do (ARB) shader-based map rendering
		return false;
	}

	// with advFade, submerged transparent objects are clipped against GL_CLIP_PLANE3
	const char* vertexProgNamesARB[2] = {
		"ARB/units3o.vp",
		"ARB/units3o2.vp",
	};

	#define sh shaderHandler
	modelShaders[MODEL_SHADER_BASIC] = sh->CreateProgramObject("[UnitDrawer]", "S3OShaderDefARB", true);
	modelShaders[MODEL_SHADER_BASIC]->AttachShaderObject(sh->CreateShaderObject(vertexProgNamesARB[ud->advFade], "", GL_VERTEX_PROGRAM_ARB));
	modelShaders[MODEL_SHADER_BASIC]->AttachShaderObject(sh->CreateShaderObject("ARB/units3o.fp", "", GL_FRAGMENT_PROGRAM_ARB));
	modelShaders[MODEL_SHADER_BASIC]->Link();

	modelShaders[MODEL_SHADER_SHADOW] = sh->CreateProgramObject("[UnitDrawer]", "S3OShaderAdvARB", true);
	modelShaders[MODEL_SHADER_SHADOW]->AttachShaderObject(sh->CreateShaderObject(vertexProgNamesARB[ud->advFade], "", GL_VERTEX_PROGRAM_ARB));
	modelShaders[MODEL_SHADER_SHADOW]->AttachShaderObject(sh->CreateShaderObject("ARB/units3o_shadow.fp", "", GL_FRAGMENT_PROGRAM_ARB));
	modelShaders[MODEL_SHADER_SHADOW]->Link();

	// make the active shader non-NULL
	if (shadowHandler->shadowsLoaded) {
		modelShaders[MODEL_SHADER_ACTIVE] = modelShaders[MODEL_SHADER_SHADOW];
	} else {
		modelShaders[MODEL_SHADER_ACTIVE] = modelShaders[MODEL_SHADER_BASIC];
	}

	#undef sh
	return true;
}

void UnitDrawerStateARB::Kill() {
	modelShaders.clear();
	shaderHandler->ReleaseProgramObjects("[UnitDrawer]");
}

bool UnitDrawerStateARB::CanEnable(const CUnitDrawer* ud) const {
	return (ud->advShading && !water->IsDrawReflection());
}

void UnitDrawerStateARB::Enable(const CUnitDrawer* ud) {
	EnableCommon(ud);

	modelShaders[MODEL_SHADER_ACTIVE]->SetUniformTarget(GL_VERTEX_PROGRAM_ARB);
	modelShaders[MODEL_SHADER_ACTIVE]->SetUniform4fv(10, &sky->GetLight()->GetLightDir().x);
	modelShaders[MODEL_SHADER_ACTIVE]->SetUniform4f(11, ud->unitSunColor.x, ud->unitSunColor.y, ud->unitSunColor.z, 0.0f);
	modelShaders[MODEL_SHADER_ACTIVE]->SetUniform4f(12, ud->unitAmbientColor.x, ud->unitAmbientColor.y, ud->unitAmbientColor.z, 1.0f); //!
	modelShaders[MODEL_SHADER_ACTIVE]->SetUniform4f(13, camera->GetPos().x, camera->GetPos().y, camera->GetPos().z, 0.0f);
	modelShaders[MODEL_SHADER_ACTIVE]->SetUniformTarget(GL_FRAGMENT_PROGRAM_ARB);
	modelShaders[MODEL_SHADER_ACTIVE]->SetUniform4f(10, 0.0f, 0.0f, 0.0f, sky->GetLight()->GetUnitShadowDensity());
	modelShaders[MODEL_SHADER_ACTIVE]->SetUniform4f(11, ud->unitAmbientColor.x, ud->unitAmbientColor.y, ud->unitAmbientColor.z, 1.0f);

	glMatrixMode(GL_MATRIX0_ARB);
	glLoadMatrixf(shadowHandler->shadowMatrix);
	glMatrixMode(GL_MODELVIEW);
}

void UnitDrawerStateARB::Disable(const CUnitDrawer* ud) {
	modelShaders[MODEL_SHADER_ACTIVE]->Disable();
	DisableCommon(ud);
}


void UnitDrawerStateARB::EnableTextures(const CUnitDrawer* ud) { EnableTexturesCommon(ud); }
void UnitDrawerStateARB::DisableTextures(const CUnitDrawer* ud) { DisableTexturesCommon(ud); }

void UnitDrawerStateARB::EnableShaders(const CUnitDrawer*) { modelShaders[MODEL_SHADER_ACTIVE]->Enable(); }
void UnitDrawerStateARB::DisableShaders(const CUnitDrawer*) { modelShaders[MODEL_SHADER_ACTIVE]->Disable(); }


void UnitDrawerStateARB::SetTeamColor(int team, float alpha) const {
	const CTeam* t = teamHandler->Team(team);
	const float4 c = float4(t->color[0] / 255.0f, t->color[1] / 255.0f, t->color[2] / 255.0f, alpha);

	// NOTE:
	//   UnitDrawer::DrawCloakedUnits and FeatureDrawer::DrawFadeFeatures
	//   disable advShading so shader is NOT always bound when team-color
	//   gets set (but the state instance is not changed to FFP! --> does
	//   not matter since Enable* and Disable* are not called)
	if (modelShaders[MODEL_SHADER_ACTIVE]->IsBound()) {
		modelShaders[MODEL_SHADER_ACTIVE]->SetUniformTarget(GL_FRAGMENT_PROGRAM_ARB);
		modelShaders[MODEL_SHADER_ACTIVE]->SetUniform4fv(14, &c[0]);
	} else {
		CUnitDrawer::SetBasicTeamColour(team, alpha);
	}

	#if 0
	if (LUA_DRAWING) {
		CUnitDrawer::SetBasicTeamColour(team, alpha);
	}
	#endif
}




bool UnitDrawerStateGLSL::Init(const CUnitDrawer* ud) {
	modelShaders.resize(MODEL_SHADER_LAST, NULL);

	if (!globalRendering->haveGLSL) {
		// not possible to do (GLSL) shader-based model rendering
		return false;
	}
	if (!configHandler->GetBool("AdvUnitShading")) {
		// not allowed to do (GLSL) shader-based map rendering
		return false;
	}

	#define sh shaderHandler

	const GL::LightHandler* lightHandler = ud->GetLightHandler();
	const std::string shaderNames[2] = {
		"S3OShaderDefGLSL",
		"S3OShaderAdvGLSL",
	};
	const std::string shaderDefs[2] = {
		"// #define use_shadows\n",
		"#define use_shadows\n",
	};
	const std::string extraDefs =
		("#define BASE_DYNAMIC_MODEL_LIGHT " + IntToString(lightHandler->GetBaseLight()) + "\n") +
		("#define MAX_DYNAMIC_MODEL_LIGHTS " + IntToString(lightHandler->GetMaxLights()) + "\n");

	for (unsigned int n = MODEL_SHADER_BASIC; n <= MODEL_SHADER_SHADOW; n++) {
		modelShaders[n] = sh->CreateProgramObject("[UnitDrawer]", shaderNames[n], false);
		modelShaders[n]->AttachShaderObject(sh->CreateShaderObject("GLSL/ModelVertProg.glsl", shaderDefs[n] + extraDefs, GL_VERTEX_SHADER));
		modelShaders[n]->AttachShaderObject(sh->CreateShaderObject("GLSL/ModelFragProg.glsl", shaderDefs[n] + extraDefs, GL_FRAGMENT_SHADER));
		modelShaders[n]->Link();
		modelShaders[n]->SetUniformLocation("diffuseTex");        // idx  0 (t1: diffuse + team-color)
		modelShaders[n]->SetUniformLocation("shadingTex");        // idx  1 (t2: spec/refl + self-illum)
		modelShaders[n]->SetUniformLocation("shadowTex");         // idx  2
		modelShaders[n]->SetUniformLocation("reflectTex");        // idx  3 (cube)
		modelShaders[n]->SetUniformLocation("specularTex");       // idx  4 (cube)
		modelShaders[n]->SetUniformLocation("sunDir");            // idx  5
		modelShaders[n]->SetUniformLocation("cameraPos");         // idx  6
		modelShaders[n]->SetUniformLocation("cameraMat");         // idx  7
		modelShaders[n]->SetUniformLocation("cameraMatInv");      // idx  8
		modelShaders[n]->SetUniformLocation("teamColor");         // idx  9
		modelShaders[n]->SetUniformLocation("sunAmbient");        // idx 10
		modelShaders[n]->SetUniformLocation("sunDiffuse");        // idx 11
		modelShaders[n]->SetUniformLocation("shadowDensity");     // idx 12
		modelShaders[n]->SetUniformLocation("shadowMatrix");      // idx 13
		modelShaders[n]->SetUniformLocation("shadowParams");      // idx 14
		modelShaders[n]->SetUniformLocation("numModelDynLights"); // idx 15

		modelShaders[n]->Enable();
		modelShaders[n]->SetUniform1i(0, 0); // diffuseTex  (idx 0, texunit 0)
		modelShaders[n]->SetUniform1i(1, 1); // shadingTex  (idx 1, texunit 1)
		modelShaders[n]->SetUniform1i(2, 2); // shadowTex   (idx 2, texunit 2)
		modelShaders[n]->SetUniform1i(3, 3); // reflectTex  (idx 3, texunit 3)
		modelShaders[n]->SetUniform1i(4, 4); // specularTex (idx 4, texunit 4)
		modelShaders[n]->SetUniform3fv(5, &sky->GetLight()->GetLightDir().x);
		modelShaders[n]->SetUniform3fv(10, &ud->unitAmbientColor[0]);
		modelShaders[n]->SetUniform3fv(11, &ud->unitSunColor[0]);
		modelShaders[n]->SetUniform1f(12, sky->GetLight()->GetUnitShadowDensity());
		modelShaders[n]->SetUniform1i(15, 0); // numModelDynLights
		modelShaders[n]->Disable();
		modelShaders[n]->Validate();
	}

	// make the active shader non-NULL
	if (shadowHandler->shadowsLoaded) {
		modelShaders[MODEL_SHADER_ACTIVE] = modelShaders[MODEL_SHADER_SHADOW];
	} else {
		modelShaders[MODEL_SHADER_ACTIVE] = modelShaders[MODEL_SHADER_BASIC];
	}

	#undef sh
	return true;
}

void UnitDrawerStateGLSL::Kill() {
	modelShaders.clear();
	shaderHandler->ReleaseProgramObjects("[UnitDrawer]");
}

bool UnitDrawerStateGLSL::CanEnable(const CUnitDrawer* ud) const {
	return (ud->advShading && !water->IsDrawReflection());
}

void UnitDrawerStateGLSL::Enable(const CUnitDrawer* ud) {
	EnableCommon(ud);

	modelShaders[MODEL_SHADER_ACTIVE]->SetUniform3fv(6, &camera->GetPos()[0]);
	modelShaders[MODEL_SHADER_ACTIVE]->SetUniformMatrix4fv(7, false, camera->GetViewMatrix());
	modelShaders[MODEL_SHADER_ACTIVE]->SetUniformMatrix4fv(8, false, camera->GetViewMatrixInverse());
	modelShaders[MODEL_SHADER_ACTIVE]->SetUniformMatrix4fv(13, false, shadowHandler->shadowMatrix);
	modelShaders[MODEL_SHADER_ACTIVE]->SetUniform4fv(14, &(shadowHandler->GetShadowParams().x));

	const_cast<GL::LightHandler*>(ud->GetLightHandler())->Update(modelShaders[MODEL_SHADER_ACTIVE]);
}

void UnitDrawerStateGLSL::Disable(const CUnitDrawer* ud) {
	modelShaders[MODEL_SHADER_ACTIVE]->Disable();
	DisableCommon(ud);
}


void UnitDrawerStateGLSL::EnableTextures(const CUnitDrawer* ud) { EnableTexturesCommon(ud); }
void UnitDrawerStateGLSL::DisableTextures(const CUnitDrawer* ud) { DisableTexturesCommon(ud); }

void UnitDrawerStateGLSL::EnableShaders(const CUnitDrawer*) { modelShaders[MODEL_SHADER_ACTIVE]->Enable(); }
void UnitDrawerStateGLSL::DisableShaders(const CUnitDrawer*) { modelShaders[MODEL_SHADER_ACTIVE]->Disable(); }


void UnitDrawerStateGLSL::UpdateCurrentShader(const CUnitDrawer* ud, const ISkyLight* skyLight) const {
	const float3 modUnitSunColor = ud->unitSunColor * skyLight->GetLightIntensity();
	const float3 sunDir = skyLight->GetLightDir();

	modelShaders[MODEL_SHADER_SHADOW]->Enable();
	modelShaders[MODEL_SHADER_SHADOW]->SetUniform3fv(5, &sunDir.x);
	modelShaders[MODEL_SHADER_SHADOW]->SetUniform1f(12, skyLight->GetUnitShadowDensity());
	modelShaders[MODEL_SHADER_SHADOW]->SetUniform3fv(11, &modUnitSunColor.x);
	modelShaders[MODEL_SHADER_SHADOW]->Disable();
}

void UnitDrawerStateGLSL::SetTeamColor(int team, float alpha) const {
	const CTeam* t = teamHandler->Team(team);
	const float4 c = float4(t->color[0] / 255.0f, t->color[1] / 255.0f, t->color[2] / 255.0f, alpha);

	// NOTE:
	//   UnitDrawer::DrawCloakedUnits and FeatureDrawer::DrawFadeFeatures
	//   disable advShading so shader is NOT always bound when team-color
	//   gets set (but the state instance is not changed to FFP! --> does
	//   not matter since Enable* and Disable* are not called)
	if (modelShaders[MODEL_SHADER_ACTIVE]->IsBound()) {
		modelShaders[MODEL_SHADER_ACTIVE]->SetUniform4fv(9, &c[0]);
	} else {
		CUnitDrawer::SetBasicTeamColour(team, alpha);
	}

	#if 0
	if (LUA_DRAWING) {
		CUnitDrawer::SetBasicTeamColour(team, alpha);
	}
	#endif
}

