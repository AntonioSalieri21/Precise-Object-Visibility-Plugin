#include "Test.h"
#include "SimpleTestModule/Public/Test/Test.h"
#include "PixelShaderUtils.h"
#include "MeshPassProcessor.inl"
#include "StaticMeshResources.h"
#include "DynamicMeshBuilder.h"
#include "RenderGraphResources.h"
#include "GlobalShader.h"
#include "UnifiedBuffer.h"
#include "CanvasTypes.h"
#include "MeshDrawShaderBindings.h"
#include "RHIGPUReadback.h"
#include "MeshPassUtils.h"
#include "MaterialShader.h"
#include "RHI.h"

using std::string;

DECLARE_STATS_GROUP(TEXT("Test"), STATGROUP_Test, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Test Execute"), STAT_Test_Execute, STATGROUP_Test);

// This class carries our parameter declarations and acts as the bridge between cpp and HLSL.
class SIMPLETESTMODULE_API FTest: public FGlobalShader
{
public:
	
	DECLARE_GLOBAL_SHADER(FTest);
	// Generates a constructor which will connect shader to register FParameter of input values
	SHADER_USE_PARAMETER_STRUCT(FTest, FGlobalShader);
	
	
	class FTest_Perm_TEST : SHADER_PERMUTATION_INT("TEST", 1);
	using FPermutationDomain = TShaderPermutationDomain<
		FTest_Perm_TEST
	>;
	// Makros to generate C++ struct of input values into shader, and connect it to RDG
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		/*
		* Here's where you define one or more of the input parameters for your shader.
		* Some examples:
		*/
		// SHADER_PARAMETER(uint32, MyUint32) // On the shader side: uint32 MyUint32;
		// SHADER_PARAMETER(FVector3f, MyVector) // On the shader side: float3 MyVector;

		// SHADER_PARAMETER_TEXTURE(Texture2D, MyTexture) // On the shader side: Texture2D<float4> MyTexture; (float4 should be whatever you expect each pixel in the texture to be, in this case float4(R,G,B,A) for 4 channels)
		// SHADER_PARAMETER_SAMPLER(SamplerState, MyTextureSampler) // On the shader side: SamplerState MySampler; // CPP side: TStaticSamplerState<ESamplerFilter::SF_Bilinear>::GetRHI();

		// SHADER_PARAMETER_ARRAY(float, MyFloatArray, [3]) // On the shader side: float MyFloatArray[3];

		// SHADER_PARAMETER_UAV(RWTexture2D<FVector4f>, MyTextureUAV) // On the shader side: RWTexture2D<float4> MyTextureUAV;
		// SHADER_PARAMETER_UAV(RWStructuredBuffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: RWStructuredBuffer<FMyCustomStruct> MyCustomStructs;
		// SHADER_PARAMETER_UAV(RWBuffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: RWBuffer<FMyCustomStruct> MyCustomStructs;

		// SHADER_PARAMETER_SRV(StructuredBuffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: StructuredBuffer<FMyCustomStruct> MyCustomStructs;
		// SHADER_PARAMETER_SRV(Buffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: Buffer<FMyCustomStruct> MyCustomStructs;
		// SHADER_PARAMETER_SRV(Texture2D<FVector4f>, MyReadOnlyTexture) // On the shader side: Texture2D<float4> MyReadOnlyTexture;

		// SHADER_PARAMETER_STRUCT_REF(FMyCustomStruct, MyCustomStruct)

		// Try to pass StencilRender here
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, CameraTexture)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int>, Output)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, Luminance)
		

	END_SHADER_PARAMETER_STRUCT()

public:
	// Shaders may have permutations (variations) based on its settings
	// F.e we can change what features are used: tesselation, translucency
	// Also it allows for changing platforms and other stuff automatically by the engine, instead of writing billion variations of the same shader
	// This function specifies what permutations to compile
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		// This line gets specific permutation from settings of FGlobalShaderPermutationParameters
		const FPermutationDomain PermutationVector(Parameters.PermutationId);
		
		// Normally this function filters out unwanted permutations, but in this basic case it allows to compile any perm
		return true;
	}
	// Allows to set compiler flags, define constants and enable specific features
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		const FPermutationDomain PermutationVector(Parameters.PermutationId);

		/*
		* Here you define constants that can be used statically in the shader code.
		* Example:
		*/
		// OutEnvironment.SetDefine(TEXT("MY_CUSTOM_CONST"), TEXT("1"));

		/*
		* These defines are used in the thread count section of our shader
		*/
		OutEnvironment.SetDefine(TEXT("THREADS_X"), NUM_THREADS_Test_X);
		OutEnvironment.SetDefine(TEXT("THREADS_Y"), NUM_THREADS_Test_Y);
		OutEnvironment.SetDefine(TEXT("THREADS_Z"), NUM_THREADS_Test_Z);

		// This shader must support typed UAV load and we are testing if it is supported at runtime using RHIIsTypedUAVLoadSupported
		//OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);

		// FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}
private:
};



FRDGTextureRef FTestInterface::RegisterRenderTarget(UTextureRenderTarget2D* RenderTarget, FRDGBuilder& GraphBuilder, string VariableName)
{

	if (!RenderTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("RenderTarget is null."));
		
	}
	const FTextureRenderTargetResource* RTResource = RenderTarget->GetRenderTargetResource();
	if (!RTResource)
	{
		UE_LOG(LogTemp, Warning, TEXT("RTResource is null."));
		
	}

	FRHITexture* TextureRHI = RTResource->GetRenderTargetTexture();
	if (!TextureRHI)
	{
		UE_LOG(LogTemp, Warning, TEXT("TextureRHI is null."));
		
	}

	FSceneRenderTargetItem RenderTargetItem;
	RenderTargetItem.TargetableTexture = TextureRHI;
	RenderTargetItem.ShaderResourceTexture = TextureRHI;

	FPooledRenderTargetDesc RenderTargetDesc = FPooledRenderTargetDesc::Create2DDesc(
		RTResource->GetSizeXY(),        // Texture resolution 
		TextureRHI->GetFormat(),             // Pixel format 
		FClearValueBinding::Black,                // Initial clear value
		TexCreate_None,
		TexCreate_RenderTargetable |              // Can be used as a render target
		TexCreate_ShaderResource |                // Can be sampled in shaders
		TexCreate_UAV,                            // Can be written via UAV (compute shaders)
		false
	);
	TRefCountPtr<IPooledRenderTarget> PooledRenderTarget;
	GRenderTargetPool.CreateUntrackedElement(
		RenderTargetDesc,
		PooledRenderTarget,
		RenderTargetItem         // Links to existing RenderTargetRHI
	);
	FString tmp = UTF8_TO_TCHAR(VariableName.c_str());
	FRDGTextureRef RDGInputTexture =
		GraphBuilder.RegisterExternalTexture(PooledRenderTarget, *tmp);

	return RDGInputTexture;

}

// This will tell the engine to create the shader and where the shader entry point is.
//                            ShaderType                            ShaderPath                     Shader function name    Type
IMPLEMENT_GLOBAL_SHADER(FTest, "/SimpleTestModuleShaders/Test/Test.usf", "Test", SF_Compute);

// Here we prepare Pass Parameters to a shader 
void FTestInterface::DispatchRenderThread(FRHICommandListImmediate& RHICmdList, FTestDispatchParams Params, TFunction<void(int OutputVal, float ObjectLuminance, float OtherLuminance)> AsyncCallback) {
	FRDGBuilder GraphBuilder(RHICmdList);

	{
		
		SCOPE_CYCLE_COUNTER(STAT_Test_Execute);
		DECLARE_GPU_STAT(Test)
		RDG_EVENT_SCOPE(GraphBuilder, "Test");
		RDG_GPU_STAT_SCOPE(GraphBuilder, Test);
		

		typename FTest::FPermutationDomain PermutationVector;
		
		// Add any static permutation options here
		// PermutationVector.Set<FTest::FMyPermutationName>(12345);

		TShaderMapRef<FTest> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);
		
		UE_LOG(LogTemp, Warning, TEXT("Check validity of a shader."));
		bool bIsShaderValid = ComputeShader.IsValid();

		if (bIsShaderValid) 
		{
			UE_LOG(LogTemp, Warning, TEXT("Shader is valid."));
			// Init pass parameters to a shader

			FTest::FParameters* PassParameters = GraphBuilder.AllocParameters<FTest::FParameters>();

			if (!Params.InputTexture)
			{
				UE_LOG(LogTemp, Warning, TEXT("InputTexture is null."));
				return;

			}
			FRDGTextureRef InputTextureRef = RegisterRenderTarget(Params.InputTexture, GraphBuilder, "InputTexture");
			PassParameters->InputTexture = InputTextureRef;
			UE_LOG(LogTemp, Warning, TEXT("Put InputTextureRef into PassParameters"));

			FRDGTextureRef CameraTextureRef = RegisterRenderTarget(Params.CameraTexture, GraphBuilder, "CameraTexture");
			PassParameters->CameraTexture = CameraTextureRef;
			UE_LOG(LogTemp, Warning, TEXT("Put CameraTextureRef into PassParameters"));
			
			
			FRDGBufferRef OutputBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateBufferDesc(sizeof(int32), 1),
				TEXT("OutputBuffer"));

			FRDGBufferRef LuminanceBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateBufferDesc(sizeof(float), 2),
				TEXT("LuminanceBuffer"));

			PassParameters->Output = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutputBuffer, PF_R32_SINT));
			PassParameters->Luminance = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(LuminanceBuffer, PF_R32_FLOAT));
			AddClearUAVPass(GraphBuilder, PassParameters->Output, 0);
			AddClearUAVPass(GraphBuilder, PassParameters->Luminance, 15.0f);
			//auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(Params.X, Params.Y, Params.Z), FComputeShaderUtils::kGolden2DGroupSize);
			FIntPoint TextureSize = InputTextureRef->Desc.Extent;
			FIntVector GroupCount(
				FMath::DivideAndRoundUp(TextureSize.X, 32),
				FMath::DivideAndRoundUp(TextureSize.Y, 32),
				1
			);
			// Binding of pass parameters to RDG, so it will automatically send data to shader
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteTest"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
			{
				FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
			});

			// GPU Readback
			FRHIGPUBufferReadback* GPUOutputBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteTestOutput"));
			FRHIGPUBufferReadback* GPULuminanceBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteTestOutput1"));
			AddEnqueueCopyPass(GraphBuilder, GPUOutputBufferReadback, OutputBuffer, 0u);
			AddEnqueueCopyPass(GraphBuilder, GPULuminanceBufferReadback, LuminanceBuffer, 0u);

			auto RunnerFunc = [GPUOutputBufferReadback, GPULuminanceBufferReadback, AsyncCallback](auto&& RunnerFunc) -> void {
				if (GPUOutputBufferReadback->IsReady() && GPULuminanceBufferReadback->IsReady()) {
					
					int32* Buffer = (int32*)GPUOutputBufferReadback->Lock(1);
					int32 ObjectSize = Buffer[0];
					
					GPUOutputBufferReadback->Unlock();

					float* LumBuffer = (float*)GPULuminanceBufferReadback->Lock(sizeof(float)*2);
					float ObjectLum = LumBuffer[0];
					float OtherLum = LumBuffer[1];
					GPULuminanceBufferReadback->Unlock();

					AsyncTask(ENamedThreads::GameThread, [AsyncCallback, ObjectSize, ObjectLum, OtherLum]() {
						AsyncCallback(ObjectSize, ObjectLum, OtherLum);
					});

					delete GPUOutputBufferReadback;
					delete GPULuminanceBufferReadback;
				} else {
					AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]() {
						RunnerFunc(RunnerFunc);
					});
				}
			};

			AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]() {
				RunnerFunc(RunnerFunc);
			});
			
		} else {
			#if WITH_EDITOR
				GEngine->AddOnScreenDebugMessage((uint64)42145125184, 6.f, FColor::Red, FString(TEXT("The compute shader has a problem.")));
			#endif

			// We exit here as we don't want to crash the game if the shader is not found or has an error.
			
		}
	}

	GraphBuilder.Execute();
}