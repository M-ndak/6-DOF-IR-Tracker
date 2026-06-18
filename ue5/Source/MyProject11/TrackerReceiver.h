#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "TrackerReceiver.generated.h"

UCLASS()
class MYPROJECT11_API ATrackerReceiver : public AActor
{
    GENERATED_BODY()

public:
    ATrackerReceiver();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracker")
    int32 ListenPort = 12345;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracker")
    float PositionScale = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tracker")
    UStaticMeshComponent* TrackerMesh;

    UPROPERTY(BlueprintReadOnly, Category = "Tracker") float X = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Tracker") float Y = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Tracker") float Z = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Tracker") float Roll = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Tracker") float Pitch = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Tracker") float Yaw = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Tracker") bool bTracking = false;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float DeltaTime) override;

private:
    FSocket* ListenSocket = nullptr;
    TArray<uint8> RecvBuffer;

    void OpenSocket();
    void ParseAndApply(const FString& JsonStr);
};