// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightGridInjection.cpp
=============================================================================*/

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "RendererInterface.h"
#include "EngineDefines.h"
#include "PrimitiveSceneProxy.h"
#include "Shader.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "LightSceneInfo.h"
#include "GlobalShader.h"
#include "SceneRendering.h"
#include "DeferredShadingRenderer.h"
#include "BasePassRendering.h"
#include "RendererModule.h"
#include "ScenePrivate.h"
#include "ClearQuad.h"
#include "VolumetricFog.h"
#include "Components/LightComponent.h"
#include "Engine/MapBuildDataRegistry.h"

int32 GLightGridPixelSize = 64;
FAutoConsoleVariableRef CVarLightGridPixelSize(
	TEXT("r.Forward.LightGridPixelSize"),
	GLightGridPixelSize,
	TEXT("Size of a cell in the light grid, in pixels."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GLightGridSizeZ = 32;
FAutoConsoleVariableRef CVarLightGridSizeZ(
	TEXT("r.Forward.LightGridSizeZ"),
	GLightGridSizeZ,
	TEXT("Number of Z slices in the light grid."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GMaxCulledLightsPerCell = 32;
FAutoConsoleVariableRef CVarMaxCulledLightsPerCell(
	TEXT("r.Forward.MaxCulledLightsPerCell"),
	GMaxCulledLightsPerCell,
	TEXT("Controls how much memory is allocated for each cell for light culling.  When r.Forward.LightLinkedListCulling is enabled, this is used to compute a global max instead of a per-cell limit on culled lights."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GLightLinkedListCulling = 1;
FAutoConsoleVariableRef CVarLightLinkedListCulling(
	TEXT("r.Forward.LightLinkedListCulling"),
	GLightLinkedListCulling,
	TEXT("Uses a reverse linked list to store culled lights, removing the fixed limit on how many lights can affect a cell - it becomes a global limit instead."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FForwardGlobalLightData,TEXT("ForwardGlobalLightData"));
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FInstancedForwardGlobalLightData, TEXT("InstancedForwardGlobalLightData"));

FForwardGlobalLightData::FForwardGlobalLightData()
{
	NumLocalLights = 0;
	HasDirectionalLight = 0;
	NumDirectionalLightCascades = 0;
	CascadeEndDepths = FVector4(0, 0, 0, 0);
	DirectionalLightShadowmapAtlas = GBlackTexture->TextureRHI;
	ShadowmapSampler = TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
	DirectionalLightUseStaticShadowing = 0;
	DirectionalLightStaticShadowmap = GBlackTexture->TextureRHI;
	StaticShadowmapSampler = TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
}

int32 NumCulledLightsGridStride = 2;
int32 NumCulledGridPrimitiveTypes = 2;
int32 LightLinkStride = 2;

// @todo Metal lacks SRV format conversions.
#if !PLATFORM_MAC && !PLATFORM_IOS
// 65k indexable light limit
typedef uint16 FLightIndexType;
#else
// UINT_MAX indexable light limit
typedef uint32 FLightIndexType;
#endif

/**  */
class FForwardCullingParameters
{
public:

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("LIGHT_LINK_STRIDE"), LightLinkStride);
	}

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		NextCulledLightLink.Bind(ParameterMap, TEXT("NextCulledLightLink"));
		StartOffsetGrid.Bind(ParameterMap, TEXT("StartOffsetGrid"));
		CulledLightLinks.Bind(ParameterMap, TEXT("CulledLightLinks"));
		NextCulledLightData.Bind(ParameterMap, TEXT("NextCulledLightData"));
	}

	template<typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, const ShaderRHIParamRef& ShaderRHI, const FViewInfo& View)
	{
		NextCulledLightLink.SetBuffer(RHICmdList, ShaderRHI, View.ForwardLightingResources->NextCulledLightLink);
		StartOffsetGrid.SetBuffer(RHICmdList, ShaderRHI, View.ForwardLightingResources->StartOffsetGrid);
		CulledLightLinks.SetBuffer(RHICmdList, ShaderRHI, View.ForwardLightingResources->CulledLightLinks);
		NextCulledLightData.SetBuffer(RHICmdList, ShaderRHI, View.ForwardLightingResources->NextCulledLightData);
	}

	template<typename ShaderRHIParamRef>
	void UnsetParameters(FRHICommandList& RHICmdList, const ShaderRHIParamRef& ShaderRHI, const FViewInfo& View)
	{
		NextCulledLightLink.UnsetUAV(RHICmdList, ShaderRHI);
		StartOffsetGrid.UnsetUAV(RHICmdList, ShaderRHI);
		CulledLightLinks.UnsetUAV(RHICmdList, ShaderRHI);
		NextCulledLightData.UnsetUAV(RHICmdList, ShaderRHI);

		TArray<FUnorderedAccessViewRHIParamRef, TInlineAllocator<4>> OutUAVs;

		if (NextCulledLightLink.IsUAVBound())
		{
			OutUAVs.Add(View.ForwardLightingResources->NextCulledLightLink.UAV);
		}

		if (StartOffsetGrid.IsUAVBound())
		{
			OutUAVs.Add(View.ForwardLightingResources->StartOffsetGrid.UAV);
		}

		if (CulledLightLinks.IsUAVBound())
		{
			OutUAVs.Add(View.ForwardLightingResources->CulledLightLinks.UAV);
		}

		if (NextCulledLightData.IsUAVBound())
		{
			OutUAVs.Add(View.ForwardLightingResources->NextCulledLightData.UAV);
		}

		if (OutUAVs.Num() > 0)
		{
			RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, OutUAVs.GetData(), OutUAVs.Num());
		}
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FForwardCullingParameters& P)
	{
		Ar << P.NextCulledLightLink;
		Ar << P.StartOffsetGrid;
		Ar << P.CulledLightLinks;
		Ar << P.NextCulledLightData;
		return Ar;
	}

private:

	FRWShaderParameter NextCulledLightLink;
	FRWShaderParameter StartOffsetGrid;
	FRWShaderParameter CulledLightLinks;
	FRWShaderParameter NextCulledLightData;
};


uint32 LightGridInjectionGroupSize = 4;

template<bool bLightLinkedListCulling>
class TLightGridInjectionCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TLightGridInjectionCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), LightGridInjectionGroupSize);
		FForwardLightingParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
		FForwardCullingParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_LINKED_CULL_LIST"), bLightLinkedListCulling);
	}

	TLightGridInjectionCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ForwardLightingParameters.Bind(Initializer.ParameterMap);
		ForwardCullingParameters.Bind(Initializer.ParameterMap);
	}

	TLightGridInjectionCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		ForwardLightingParameters.Set(RHICmdList, ShaderRHI, View);
		ForwardCullingParameters.Set(RHICmdList, ShaderRHI, View);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FViewInfo& View)
	{
		ForwardLightingParameters.UnsetParameters(RHICmdList, GetComputeShader(), View);
		ForwardCullingParameters.UnsetParameters(RHICmdList, GetComputeShader(), View);
	}

	virtual bool Serialize(FArchive& Ar)
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ForwardLightingParameters;
		Ar << ForwardCullingParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FForwardLightingParameters ForwardLightingParameters;
	FForwardCullingParameters ForwardCullingParameters;
};

IMPLEMENT_SHADER_TYPE(template<>,TLightGridInjectionCS<true>,TEXT("LightGridInjection"),TEXT("LightGridInjectionCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TLightGridInjectionCS<false>,TEXT("LightGridInjection"),TEXT("LightGridInjectionCS"),SF_Compute);

class FLightGridCompactCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FLightGridCompactCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), LightGridInjectionGroupSize);
		FForwardLightingParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
		FForwardCullingParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("MAX_CAPTURES"), GMaxNumReflectionCaptures);
	}

	FLightGridCompactCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ForwardLightingParameters.Bind(Initializer.ParameterMap);
		ForwardCullingParameters.Bind(Initializer.ParameterMap);
	}

	FLightGridCompactCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		ForwardLightingParameters.Set(RHICmdList, ShaderRHI, View);
		ForwardCullingParameters.Set(RHICmdList, ShaderRHI, View);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FViewInfo& View)
	{
		ForwardLightingParameters.UnsetParameters(RHICmdList, GetComputeShader(), View);
		ForwardCullingParameters.UnsetParameters(RHICmdList, GetComputeShader(), View);
	}

	virtual bool Serialize(FArchive& Ar)
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ForwardLightingParameters;
		Ar << ForwardCullingParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FForwardLightingParameters ForwardLightingParameters;
	FForwardCullingParameters ForwardCullingParameters;
};

IMPLEMENT_SHADER_TYPE(,FLightGridCompactCS,TEXT("LightGridInjection"),TEXT("LightGridCompactCS"),SF_Compute);

FVector GetLightGridZParams(float NearPlane, float FarPlane)
{
	// S = distribution scale
	// B, O are solved for given the z distances of the first+last slice, and the # of slices.
	//
	// slice = log2(z*B + O) * S

	// Don't spend lots of resolution right in front of the near plane
	double NearOffset = .095 * 100;
	// Space out the slices so they aren't all clustered at the near plane
	double S = 4.05;

	double N = NearPlane + NearOffset;
	double F = FarPlane;

	double O = (F - N * exp2((GLightGridSizeZ - 1) / S)) / (F - N);
	double B = (1 - O) / N;

	return FVector(B, O, S);
}

void FDeferredShadingSceneRenderer::ComputeLightGrid(FRHICommandListImmediate& RHICmdList)
{
	if (FeatureLevel >= ERHIFeatureLevel::SM5)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ComputeLightGrid);
		SCOPED_DRAW_EVENT(RHICmdList, ComputeLightGrid);

		static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
		const bool bAllowStaticLighting = (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnRenderThread() != 0);

		bool bAnyViewUsesForwardLighting = false;

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];
			bAnyViewUsesForwardLighting |= View.bTranslucentSurfaceLighting || ShouldRenderVolumetricFog(Scene, ViewFamily);
		}

		const bool bCullLightsToGrid = (IsForwardShadingEnabled(FeatureLevel) || bAnyViewUsesForwardLighting) && ViewFamily.EngineShowFlags.DirectLighting;

		FSimpleLightArray SimpleLights;

		if (bCullLightsToGrid)
		{
			GatherSimpleLights(ViewFamily, Views, SimpleLights);
		}

		TArray<FForwardGlobalLightData, TInlineAllocator<2>> GlobalLightDataForAllViews;
		GlobalLightDataForAllViews.Empty(Views.Num());
		GlobalLightDataForAllViews.AddDefaulted(Views.Num());

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];

			FForwardGlobalLightData& GlobalLightData = GlobalLightDataForAllViews[ViewIndex];
			TArray<FForwardLocalLightData, SceneRenderingAllocator> ForwardLocalLightData;
			float FurthestLight = 1000;

			if (bCullLightsToGrid)
			{
				ForwardLocalLightData.Empty(Scene->Lights.Num());

				for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
				{
					const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
					const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;
					const FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightIt.GetIndex()];
					const FLightSceneProxy* LightProxy = LightSceneInfo->Proxy;

					if (LightSceneInfo->ShouldRenderLightViewIndependent()
						&& LightSceneInfo->ShouldRenderLight(View)
						// Reflection override skips direct specular because it tends to be blindingly bright with a perfectly smooth surface
						&& !ViewFamily.EngineShowFlags.ReflectionOverride)
					{
						FVector4 LightPositionAndInvRadius;
						FVector4 LightColorAndFalloffExponent;
						FVector NormalizedLightDirection;
						FVector2D SpotAngles;
						float SourceRadius;
						float SourceLength;
						float MinRoughness;

						// Get the light parameters
						LightProxy->GetParameters(
							LightPositionAndInvRadius,
							LightColorAndFalloffExponent,
							NormalizedLightDirection,
							SpotAngles,
							SourceRadius,
							SourceLength,
							MinRoughness);

						if (LightProxy->IsInverseSquared())
						{
							// Correction for lumen units
							LightColorAndFalloffExponent.X *= 16.0f;
							LightColorAndFalloffExponent.Y *= 16.0f;
							LightColorAndFalloffExponent.Z *= 16.0f;
							LightColorAndFalloffExponent.W = 0;
						}

						// When rendering reflection captures, the direct lighting of the light is actually the indirect specular from the main view
						if (View.bIsReflectionCapture)
						{
							LightColorAndFalloffExponent.X *= LightProxy->GetIndirectLightingScale();
							LightColorAndFalloffExponent.Y *= LightProxy->GetIndirectLightingScale();
							LightColorAndFalloffExponent.Z *= LightProxy->GetIndirectLightingScale();
						}

						int32 ShadowMapChannel = LightProxy->GetShadowMapChannel();
						int32 DynamicShadowMapChannel = LightSceneInfo->GetDynamicShadowMapChannel();

						if (!bAllowStaticLighting)
						{
							ShadowMapChannel = INDEX_NONE;
						}

						// Static shadowing uses ShadowMapChannel, dynamic shadows are packed into light attenuation using DynamicShadowMapChannel
						uint32 ShadowMapChannelMaskPacked =
							(ShadowMapChannel == 0 ? 1 : 0) |
							(ShadowMapChannel == 1 ? 2 : 0) |
							(ShadowMapChannel == 2 ? 4 : 0) |
							(ShadowMapChannel == 3 ? 8 : 0) |
							(DynamicShadowMapChannel == 0 ? 16 : 0) |
							(DynamicShadowMapChannel == 1 ? 32 : 0) |
							(DynamicShadowMapChannel == 2 ? 64 : 0) |
							(DynamicShadowMapChannel == 3 ? 128 : 0);

						ShadowMapChannelMaskPacked |= LightProxy->GetLightingChannelMask() << 8;

						if ((LightSceneInfoCompact.LightType == LightType_Point && ViewFamily.EngineShowFlags.PointLights)
							|| (LightSceneInfoCompact.LightType == LightType_Spot && ViewFamily.EngineShowFlags.SpotLights))
						{
							ForwardLocalLightData.AddUninitialized(1);
							FForwardLocalLightData& LightData = ForwardLocalLightData.Last();

							const float LightFade = GetLightFadeFactor(View, LightProxy);
							LightColorAndFalloffExponent.X *= LightFade;
							LightColorAndFalloffExponent.Y *= LightFade;
							LightColorAndFalloffExponent.Z *= LightFade;

							LightData.LightPositionAndInvRadius = LightPositionAndInvRadius;
							LightData.LightColorAndFalloffExponent = LightColorAndFalloffExponent;
							LightData.LightDirectionAndShadowMapChannelMask = FVector4(NormalizedLightDirection, *((float*)&ShadowMapChannelMaskPacked));

							LightData.SpotAnglesAndSourceRadiusPacked = FVector4(SpotAngles.X, SpotAngles.Y, SourceRadius, 0);

							float VolumetricScatteringIntensity = LightProxy->GetVolumetricScatteringIntensity();

							if (LightNeedsSeparateInjectionIntoVolumetricFog(LightSceneInfo, VisibleLightInfos[LightSceneInfo->Id]))
							{
								// Disable this lights forward shading volumetric scattering contribution
								VolumetricScatteringIntensity = 0;
							}

							// Pack both values into a single float to keep float4 alignment
							const FFloat16 SourceLength16f = FFloat16(SourceLength);
							const FFloat16 VolumetricScatteringIntensity16f = FFloat16(VolumetricScatteringIntensity);
							const uint32 PackedWInt = ((uint32)SourceLength16f.Encoded) | ((uint32)VolumetricScatteringIntensity16f.Encoded << 16);
							LightData.SpotAnglesAndSourceRadiusPacked.W = *(float*)&PackedWInt;

							const FSphere BoundingSphere = LightProxy->GetBoundingSphere();
							const float Distance = View.ViewMatrices.GetViewMatrix().TransformPosition(BoundingSphere.Center).Z + BoundingSphere.W;
							FurthestLight = FMath::Max(FurthestLight, Distance);
						}
						else if (LightSceneInfoCompact.LightType == LightType_Directional && ViewFamily.EngineShowFlags.DirectionalLights)
						{
							GlobalLightData.HasDirectionalLight = 1;
							GlobalLightData.DirectionalLightColor = LightColorAndFalloffExponent;
							GlobalLightData.DirectionalLightVolumetricScatteringIntensity = LightProxy->GetVolumetricScatteringIntensity();
							GlobalLightData.DirectionalLightDirection = NormalizedLightDirection;
							GlobalLightData.DirectionalLightShadowMapChannelMask = ShadowMapChannelMaskPacked;

							const FVector2D FadeParams = LightProxy->GetDirectionalLightDistanceFadeParameters(View.GetFeatureLevel(), LightSceneInfo->IsPrecomputedLightingValid());

							GlobalLightData.DirectionalLightDistanceFadeMAD = FVector2D(FadeParams.Y, -FadeParams.X * FadeParams.Y);

							if (ViewFamily.EngineShowFlags.DynamicShadows && VisibleLightInfos.IsValidIndex(LightSceneInfo->Id) && VisibleLightInfos[LightSceneInfo->Id].AllProjectedShadows.Num() > 0)
							{
								const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& DirectionalLightShadowInfos = VisibleLightInfos[LightSceneInfo->Id].AllProjectedShadows;

								GlobalLightData.NumDirectionalLightCascades = 0;

								for (int32 ShadowIndex = 0; ShadowIndex < DirectionalLightShadowInfos.Num(); ShadowIndex++)
								{
									const FProjectedShadowInfo* ShadowInfo = DirectionalLightShadowInfos[ShadowIndex];
									const int32 CascadeIndex = ShadowInfo->CascadeSettings.ShadowSplitIndex;

									if (ShadowInfo->IsWholeSceneDirectionalShadow() && ShadowInfo->bAllocated && CascadeIndex < GMaxForwardShadowCascades)
									{
										GlobalLightData.NumDirectionalLightCascades++;
										GlobalLightData.DirectionalLightWorldToShadowMatrix[CascadeIndex] = ShadowInfo->GetWorldToShadowMatrix(GlobalLightData.DirectionalLightShadowmapMinMax[CascadeIndex]);
										GlobalLightData.CascadeEndDepths[CascadeIndex] = ShadowInfo->CascadeSettings.SplitFar;

										if (CascadeIndex == 0)
										{
											GlobalLightData.DirectionalLightShadowmapAtlas = ShadowInfo->RenderTargets.DepthTarget->GetRenderTargetItem().ShaderResourceTexture.GetReference();
											GlobalLightData.DirectionalLightDepthBias = ShadowInfo->GetShaderDepthBias();
										}
									}
								}
							}

							const FStaticShadowDepthMap* StaticShadowDepthMap = LightSceneInfo->Proxy->GetStaticShadowDepthMap();
							const uint32 bStaticallyShadowedValue = LightSceneInfo->IsPrecomputedLightingValid() && StaticShadowDepthMap && StaticShadowDepthMap->TextureRHI ? 1 : 0;
	
							GlobalLightData.DirectionalLightUseStaticShadowing = bStaticallyShadowedValue;
							GlobalLightData.DirectionalLightStaticShadowBufferSize = bStaticallyShadowedValue ? FVector4(StaticShadowDepthMap->Data->ShadowMapSizeX, StaticShadowDepthMap->Data->ShadowMapSizeY, 1.0f / StaticShadowDepthMap->Data->ShadowMapSizeX, 1.0f / StaticShadowDepthMap->Data->ShadowMapSizeY) : FVector4(0, 0, 0, 0);
							GlobalLightData.DirectionalLightWorldToStaticShadow = bStaticallyShadowedValue ? StaticShadowDepthMap->Data->WorldToLight : FMatrix::Identity;
							GlobalLightData.DirectionalLightStaticShadowmap = bStaticallyShadowedValue ? StaticShadowDepthMap->TextureRHI : GWhiteTexture->TextureRHI;
						}
					}
				}

				// Pack both values into a single float to keep float4 alignment
				const FFloat16 SimpleLightSourceLength16f = FFloat16(0);
				FLightingChannels SimpleLightLightingChannels;
				// Put simple lights in all lighting channels
				SimpleLightLightingChannels.bChannel0 = SimpleLightLightingChannels.bChannel1 = SimpleLightLightingChannels.bChannel2 = true;
				const uint32 SimpleLightLightingChannelMask = GetLightingChannelMaskForStruct(SimpleLightLightingChannels);

				for (int32 SimpleLightIndex = 0; SimpleLightIndex < SimpleLights.InstanceData.Num(); SimpleLightIndex++)
				{	
					ForwardLocalLightData.AddUninitialized(1);
					FForwardLocalLightData& LightData = ForwardLocalLightData.Last();

					const FSimpleLightEntry& SimpleLight = SimpleLights.InstanceData[SimpleLightIndex];
					const FSimpleLightPerViewEntry& SimpleLightPerViewData = SimpleLights.GetViewDependentData(SimpleLightIndex, ViewIndex, Views.Num());
					LightData.LightPositionAndInvRadius = FVector4(SimpleLightPerViewData.Position, 1.0f / FMath::Max(SimpleLight.Radius, KINDA_SMALL_NUMBER));
					LightData.LightColorAndFalloffExponent = FVector4(SimpleLight.Color, SimpleLight.Exponent);

					// No shadowmap channels for simple lights
					uint32 ShadowMapChannelMask = 0;
					ShadowMapChannelMask |= SimpleLightLightingChannelMask << 8;

					LightData.LightDirectionAndShadowMapChannelMask = FVector4(FVector(1, 0, 0), *((float*)&ShadowMapChannelMask));

					// Pack both values into a single float to keep float4 alignment
					const FFloat16 VolumetricScatteringIntensity16f = FFloat16(SimpleLight.VolumetricScatteringIntensity);
					const uint32 PackedWInt = ((uint32)SimpleLightSourceLength16f.Encoded) | ((uint32)VolumetricScatteringIntensity16f.Encoded << 16);
		
					LightData.SpotAnglesAndSourceRadiusPacked = FVector4(-2, 1, 0, *(float*)&PackedWInt);

					if( SimpleLight.Exponent == 0.0f )
					{
						// Correction for lumen units
						LightData.LightColorAndFalloffExponent *= 16.0f;
					}
				}
			}

			// Store off the number of lights before we add a fake entry
			const int32 NumLocalLightsFinal = ForwardLocalLightData.Num();

			if (ForwardLocalLightData.Num() == 0)
			{
				// Make sure the buffer gets created even though we're not going to read from it in the shader, for platforms like PS4 that assert on null resources being bound
				ForwardLocalLightData.AddZeroed();
			}

			{
				const uint32 NumBytesRequired = ForwardLocalLightData.Num() * ForwardLocalLightData.GetTypeSize();

				if (View.ForwardLightingResources->ForwardLocalLightBuffer.NumBytes < NumBytesRequired)
				{
					View.ForwardLightingResources->ForwardLocalLightBuffer.Release();
					View.ForwardLightingResources->ForwardLocalLightBuffer.Initialize(sizeof(FVector4), NumBytesRequired / sizeof(FVector4), PF_A32B32G32R32F, BUF_Volatile);
				}

				View.ForwardLightingResources->ForwardLocalLightBuffer.Lock();
				FPlatformMemory::Memcpy(View.ForwardLightingResources->ForwardLocalLightBuffer.MappedBuffer, ForwardLocalLightData.GetData(), ForwardLocalLightData.Num() * ForwardLocalLightData.GetTypeSize());
				View.ForwardLightingResources->ForwardLocalLightBuffer.Unlock();
			}

			const FIntPoint LightGridSizeXY = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), GLightGridPixelSize);
			GlobalLightData.NumLocalLights = NumLocalLightsFinal;
			GlobalLightData.NumReflectionCaptures = View.NumBoxReflectionCaptures + View.NumSphereReflectionCaptures;
			GlobalLightData.NumGridCells = LightGridSizeXY.X * LightGridSizeXY.Y * GLightGridSizeZ;
			GlobalLightData.CulledGridSize = FIntVector(LightGridSizeXY.X, LightGridSizeXY.Y, GLightGridSizeZ);
			GlobalLightData.MaxCulledLightsPerCell = GMaxCulledLightsPerCell;
			GlobalLightData.LightGridPixelSizeShift = FMath::FloorLog2(GLightGridPixelSize);

			// Clamp far plane to something reasonable
			float FarPlane = FMath::Min(FMath::Max(FurthestLight, View.FurthestReflectionCaptureDistance), (float)HALF_WORLD_MAX / 5.0f);
			FVector ZParams = GetLightGridZParams(View.NearClippingDistance, FarPlane + 10.f);
			GlobalLightData.LightGridZParams = ZParams;

			const uint64 NumIndexableLights = 1llu << (sizeof(FLightIndexType) * 8llu);

			if ((uint64)ForwardLocalLightData.Num() > NumIndexableLights)
			{
				static bool bWarned = false;

				if (!bWarned)
				{
					UE_LOG(LogRenderer, Warning, TEXT("Exceeded indexable light count, glitches will be visible (%u / %llu)"), ForwardLocalLightData.Num(), NumIndexableLights);
					bWarned = true;
				}
			}

			View.ForwardLightingResources->ForwardGlobalLightData = TUniformBufferRef<FForwardGlobalLightData>::CreateUniformBufferImmediate(GlobalLightData, UniformBuffer_SingleFrame);
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];
			const FForwardGlobalLightData& GlobalLightData = GlobalLightDataForAllViews[ViewIndex];

			const FIntPoint LightGridSizeXY = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), GLightGridPixelSize);
			const int32 NumCells = LightGridSizeXY.X * LightGridSizeXY.Y * GLightGridSizeZ * NumCulledGridPrimitiveTypes;

			if (View.ForwardLightingResources->NumCulledLightsGrid.NumBytes != NumCells * NumCulledLightsGridStride * sizeof(uint32))
			{
				View.ForwardLightingResources->NumCulledLightsGrid.Initialize(sizeof(uint32), NumCells * NumCulledLightsGridStride, PF_R32_UINT);
				View.ForwardLightingResources->NextCulledLightLink.Initialize(sizeof(uint32), 1, PF_R32_UINT);
				View.ForwardLightingResources->StartOffsetGrid.Initialize(sizeof(uint32), NumCells, PF_R32_UINT);
				View.ForwardLightingResources->NextCulledLightData.Initialize(sizeof(uint32), 1, PF_R32_UINT);
			}

			if (View.ForwardLightingResources->CulledLightDataGrid.NumBytes != NumCells * GMaxCulledLightsPerCell * sizeof(FLightIndexType))
			{
				View.ForwardLightingResources->CulledLightDataGrid.Initialize(sizeof(FLightIndexType), NumCells * GMaxCulledLightsPerCell, sizeof(FLightIndexType) == sizeof(uint16) ? PF_R16_UINT : PF_R32_UINT);
				View.ForwardLightingResources->CulledLightLinks.Initialize(sizeof(uint32), NumCells * GMaxCulledLightsPerCell * LightLinkStride, PF_R32_UINT);
			}

			const FIntVector NumGroups = FIntVector::DivideAndRoundUp(FIntVector(LightGridSizeXY.X, LightGridSizeXY.Y, GLightGridSizeZ), LightGridInjectionGroupSize);

			{
				SCOPED_DRAW_EVENTF(RHICmdList, CullLights, TEXT("CullLights %ux%ux%u NumLights %u NumCaptures %u"), 
					GlobalLightData.CulledGridSize.X, 
					GlobalLightData.CulledGridSize.Y,
					GlobalLightData.CulledGridSize.Z,
					GlobalLightData.NumLocalLights,
					GlobalLightData.NumReflectionCaptures);

				TArray<FUnorderedAccessViewRHIParamRef, TInlineAllocator<6>> OutUAVs;
				OutUAVs.Add(View.ForwardLightingResources->NumCulledLightsGrid.UAV);
				OutUAVs.Add(View.ForwardLightingResources->CulledLightDataGrid.UAV);
				OutUAVs.Add(View.ForwardLightingResources->NextCulledLightLink.UAV);
				OutUAVs.Add(View.ForwardLightingResources->StartOffsetGrid.UAV);
				OutUAVs.Add(View.ForwardLightingResources->CulledLightLinks.UAV);
				OutUAVs.Add(View.ForwardLightingResources->NextCulledLightData.UAV);
				RHICmdList.TransitionResources(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, OutUAVs.GetData(), OutUAVs.Num());

				if (GLightLinkedListCulling)
				{
					ClearUAV(RHICmdList, GMaxRHIFeatureLevel, View.ForwardLightingResources->StartOffsetGrid, 0xFFFFFFFF);
					ClearUAV(RHICmdList, GMaxRHIFeatureLevel, View.ForwardLightingResources->NextCulledLightLink, 0);
					ClearUAV(RHICmdList, GMaxRHIFeatureLevel, View.ForwardLightingResources->NextCulledLightData, 0);

					TShaderMapRef<TLightGridInjectionCS<true> > ComputeShader(View.ShaderMap);
					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
					ComputeShader->SetParameters(RHICmdList, View);
					DispatchComputeShader(RHICmdList, *ComputeShader, NumGroups.X, NumGroups.Y, NumGroups.Z);
					ComputeShader->UnsetParameters(RHICmdList, View);
				}
				else
				{
					ClearUAV(RHICmdList, GMaxRHIFeatureLevel, View.ForwardLightingResources->NumCulledLightsGrid, 0);

					TShaderMapRef<TLightGridInjectionCS<false> > ComputeShader(View.ShaderMap);
					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
					ComputeShader->SetParameters(RHICmdList, View);
					DispatchComputeShader(RHICmdList, *ComputeShader, NumGroups.X, NumGroups.Y, NumGroups.Z);
					ComputeShader->UnsetParameters(RHICmdList, View);
				}
			}

			if (GLightLinkedListCulling)
			{
				SCOPED_DRAW_EVENT(RHICmdList, Compact);

				TShaderMapRef<FLightGridCompactCS> ComputeShader(View.ShaderMap);
				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
				ComputeShader->SetParameters(RHICmdList, View);
				DispatchComputeShader(RHICmdList, *ComputeShader, NumGroups.X, NumGroups.Y, NumGroups.Z);
				ComputeShader->UnsetParameters(RHICmdList, View);
			}
		}
	}
}

void FDeferredShadingSceneRenderer::RenderForwardShadingShadowProjections(FRHICommandListImmediate& RHICmdList)
{
	bool bLightAttenuationNeeded = false;

	for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;
		const FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];

		bLightAttenuationNeeded = bLightAttenuationNeeded || VisibleLightInfo.ShadowsToProject.Num() > 0 || VisibleLightInfo.CapsuleShadowsToProject.Num() > 0;
	}

	FSceneRenderTargets& SceneRenderTargets = FSceneRenderTargets::Get(RHICmdList);
	SceneRenderTargets.SetLightAttenuationMode(bLightAttenuationNeeded);

	if (bLightAttenuationNeeded)
	{
		SCOPED_DRAW_EVENT(RHICmdList, ShadowProjectionOnOpaque);

		// All shadows render with min blending
		bool bClearToWhite = true;
		SceneRenderTargets.BeginRenderingLightAttenuation(RHICmdList, bClearToWhite);

		for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
		{
			const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
			const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;
			FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];

			const bool bIssueLightDrawEvent = VisibleLightInfo.ShadowsToProject.Num() > 0 || VisibleLightInfo.CapsuleShadowsToProject.Num() > 0;

			FString LightNameWithLevel;
			GetLightNameForDrawEvent(LightSceneInfo->Proxy, LightNameWithLevel);
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventLightPass, bIssueLightDrawEvent, *LightNameWithLevel);

			if (VisibleLightInfo.ShadowsToProject.Num() > 0)
			{
				FSceneRenderer::RenderShadowProjections(RHICmdList, LightSceneInfo, true, false);
			}

			RenderCapsuleDirectShadows(*LightSceneInfo, RHICmdList, VisibleLightInfo.CapsuleShadowsToProject, true);

			if (LightSceneInfo->GetDynamicShadowMapChannel() >= 0 && LightSceneInfo->GetDynamicShadowMapChannel() < 4)
			{
				RenderLightFunction(RHICmdList, LightSceneInfo, true, true);
			}
		}

		SceneRenderTargets.FinishRenderingLightAttenuation(RHICmdList);
	}
}
