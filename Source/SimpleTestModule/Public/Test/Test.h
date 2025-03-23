#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialRenderProxy.h"
#include "Engine/Texture2D.h"

#include "Test.generated.h"

using std::string;

// This is NOT input data for shader struct, but Blueprint friendly struct that takes data from game thread to render thread for further processing
// So for example we take UTextureRenderTarget2D (UObject) and transfer it to RDGTexture object, so it will be included into makro generated shader input C++ struct
struct SIMPLETESTMODULE_API FTestDispatchParams
{
	int X;
	int Y;
	int Z;
	
	UTextureRenderTarget2D* InputTexture; // Must be a RenderTarget for compute shaders
	UTextureRenderTarget2D* CameraTexture;
	int Output; 
	int ObjectLuminance;
	int OtherLuminance;

	FTestDispatchParams(int x, int y, int z, UTextureRenderTarget2D* InTexture, UTextureRenderTarget2D* CamTexture)
		: X(x), Y(y), Z(z), InputTexture(InTexture), CameraTexture(CamTexture), Output(1) {
	} 
};

// Compute Shader Interface
class SIMPLETESTMODULE_API FTestInterface {
public:
	// Executes shader on the render thread
	static void DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FTestDispatchParams Params,
		TFunction<void(int OutputVal, float ObjectLuminance, float OtherLuminance)> AsyncCallback
	);

	static FRDGTextureRef RegisterRenderTarget(UTextureRenderTarget2D* RenderTarget, FRDGBuilder& GraphBuilder, string VariableName);

	// Executes shader from the game thread
	static void DispatchGameThread(
		FTestDispatchParams Params,
		TFunction<void(int OutputVal, float ObjectLuminance, float OtherLuminance)> AsyncCallback
	)
	{
		ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
			[Params, AsyncCallback](FRHICommandListImmediate& RHICmdList)
			{
				DispatchRenderThread(RHICmdList, Params, AsyncCallback);
			});
	}

	// Dispatches shader from any thread
	static void Dispatch(
		FTestDispatchParams Params,
		TFunction<void(int OutputVal, float ObjectLuminance, float OtherLuminance)> AsyncCallback
	)
	{
		if (IsInRenderingThread()) {
			DispatchRenderThread(GetImmediateCommandList_ForRenderCommand(), Params, AsyncCallback);
		}
		else {
			DispatchGameThread(Params, AsyncCallback);
		}
	}

	//static TRefCountPtr<IPooledRenderTarget> PooledRenderTarget;
};

//DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTestLibrary_AsyncExecutionCompleted, const int, ScreenSpaceObjectSize);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnTestLibrary_AsyncExecutionCompleted, 
	const int, ScreenSpaceObjectSize, const float, ObjectLuminance, const float, OtherLuminance
);
UCLASS()
class SIMPLETESTMODULE_API UTestLibrary_AsyncExecution : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	// Executes the compute shader
	virtual void Activate() override {
		// Ensure InputTexture is a RenderTarget
		if (!InputTexture) return;
		if (!CameraTexture) return;
		// Dispatch compute shader
		FTestDispatchParams Params(1, 1, 1, InputTexture, CameraTexture);
		FTestInterface::Dispatch(Params, [this](int OutputVal, float ObjectLiminance, float OtherLuminance) {
			this->Completed.Broadcast(OutputVal, ObjectLiminance, OtherLuminance);
			});
	}

	// Blueprint function
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", Category = "ComputeShader", WorldContext = "WorldContextObject"))
	static UTestLibrary_AsyncExecution* VisibilityToneCalculation(UObject* WorldContextObject, UTextureRenderTarget2D* InputTexture, 
		UTextureRenderTarget2D* CameraTexture) {
		UTestLibrary_AsyncExecution* Action = NewObject<UTestLibrary_AsyncExecution>();
		Action->InputTexture = InputTexture;
		Action->CameraTexture = CameraTexture;
		Action->RegisterWithGameInstance(WorldContextObject);
		return Action;
	}

	UPROPERTY(BlueprintAssignable)
	FOnTestLibrary_AsyncExecutionCompleted Completed;

	// Texture input (must be a RenderTarget)
	UTextureRenderTarget2D* InputTexture;
	UTextureRenderTarget2D* CameraTexture;
};
