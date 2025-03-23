#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialRenderProxy.h"

#include "LuminanceCalculationShader.generated.h"
using std::string;
struct LUMINANCECALCULATIONMODULE_API FLuminanceCalculationShaderDispatchParams
{
	int X;
	int Y;
	int Z;

	UTextureRenderTarget2D* RenderTarget;
	int Output;
	
	FLuminanceCalculationShaderDispatchParams(int x, int y, int z, UTextureRenderTarget2D* RenderTarget)
		: X(x)
		, Y(y)
		, Z(z)
		, RenderTarget(RenderTarget)
	{
	}
};

// This is a public interface that we define so outside code can invoke our compute shader.
class LUMINANCECALCULATIONMODULE_API FLuminanceCalculationShaderInterface {
public:
	// Executes this shader on the render thread
	static void DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FLuminanceCalculationShaderDispatchParams Params,
		TFunction<void(int OutputVal)> AsyncCallback
	);
	static FRDGTextureRef RegisterRenderTarget(UTextureRenderTarget2D* RenderTarget, FRDGBuilder& GraphBuilder, string VariableName);
	// Executes this shader on the render thread from the game thread via EnqueueRenderThreadCommand
	static void DispatchGameThread(
		FLuminanceCalculationShaderDispatchParams Params,
		TFunction<void(int OutputVal)> AsyncCallback
	)
	{
		ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
		[Params, AsyncCallback](FRHICommandListImmediate& RHICmdList)
		{
			DispatchRenderThread(RHICmdList, Params, AsyncCallback);
		});
	}

	// Dispatches this shader. Can be called from any thread
	static void Dispatch(
		FLuminanceCalculationShaderDispatchParams Params,
		TFunction<void(int OutputVal)> AsyncCallback
	)
	{
		if (IsInRenderingThread()) {
			DispatchRenderThread(GetImmediateCommandList_ForRenderCommand(), Params, AsyncCallback);
		}else{
			DispatchGameThread(Params, AsyncCallback);
		}
	}
};



DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLuminanceCalculationShaderLibrary_AsyncExecutionCompleted, const int, Value);


UCLASS() // Change the _API to match your project
class LUMINANCECALCULATIONMODULE_API ULuminanceCalculationShaderLibrary_AsyncExecution : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	
	// Execute the actual load
	virtual void Activate() override {
		// Create a dispatch parameters struct and fill it the input array with our args
		
		if (!RenderTarget) return;
		FLuminanceCalculationShaderDispatchParams Params(1, 1, 1, RenderTarget);

		// Dispatch the compute shader and wait until it completes
		FLuminanceCalculationShaderInterface::Dispatch(Params, [this](int OutputVal) 
		{
			this->Completed.Broadcast(OutputVal);
		});
	}
	
	
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", Category = "ComputeShader", WorldContext = "WorldContextObject"))
	static ULuminanceCalculationShaderLibrary_AsyncExecution* BrightnessCalculation(UObject* WorldContextObject, UTextureRenderTarget2D* RenderTarget)
	{
		ULuminanceCalculationShaderLibrary_AsyncExecution* Action = NewObject<ULuminanceCalculationShaderLibrary_AsyncExecution>();
		Action->RenderTarget = RenderTarget;
		Action->RegisterWithGameInstance(WorldContextObject);

		return Action;
	}

	UPROPERTY(BlueprintAssignable)
	FOnLuminanceCalculationShaderLibrary_AsyncExecutionCompleted Completed;

	
	UTextureRenderTarget2D* RenderTarget;
};