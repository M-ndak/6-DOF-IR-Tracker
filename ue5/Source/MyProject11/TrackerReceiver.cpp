#include "TrackerReceiver.h"
#include "Networking.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

ATrackerReceiver::ATrackerReceiver()
{
    PrimaryActorTick.bCanEverTick = true;
    TrackerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrackerMesh"));
    RootComponent = TrackerMesh;
    RecvBuffer.SetNumZeroed(65535);
}

void ATrackerReceiver::BeginPlay()
{
    Super::BeginPlay();
    OpenSocket();
}

void ATrackerReceiver::OpenSocket()
{
    FIPv4Address Addr(0, 0, 0, 0);
    TSharedRef<FInternetAddr> LocalAddr =
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
    LocalAddr->SetIp(Addr.Value);
    LocalAddr->SetPort(ListenPort);

    ListenSocket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)
        ->CreateSocket(NAME_DGram, TEXT("TrackerUDP"), false);

    if (ListenSocket)
    {
        ListenSocket->SetNonBlocking(true);
        ListenSocket->SetReuseAddr(true);
        bool bBound = ListenSocket->Bind(*LocalAddr);
        UE_LOG(LogTemp, Log, TEXT("[Tracker] UDP socket bound on port %d: %s"),
            ListenPort, bBound ? TEXT("OK") : TEXT("FAILED"));
    }
}

void ATrackerReceiver::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!ListenSocket) return;

    uint32 PendingDataSize = 0;
    while (ListenSocket->HasPendingData(PendingDataSize))
    {
        int32 BytesRead = 0;
        RecvBuffer.SetNumZeroed(PendingDataSize + 1);
        ListenSocket->Recv(RecvBuffer.GetData(), RecvBuffer.Num(), BytesRead);

        if (BytesRead > 0)
        {
            FString JsonStr = FString(UTF8_TO_TCHAR(
                reinterpret_cast<const char*>(RecvBuffer.GetData())));
            ParseAndApply(JsonStr);
        }
    }
}

void ATrackerReceiver::ParseAndApply(const FString& JsonStr)
{
    TSharedPtr<FJsonObject> Json;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);

    if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
        return;

    bTracking = Json->GetBoolField(TEXT("tracking"));
    X = (float)Json->GetNumberField(TEXT("x"));
    Y = (float)Json->GetNumberField(TEXT("y"));
    Z = (float)Json->GetNumberField(TEXT("z"));
    Roll = (float)Json->GetNumberField(TEXT("roll"));
    Pitch = (float)Json->GetNumberField(TEXT("pitch"));
    Yaw = (float)Json->GetNumberField(TEXT("yaw"));

    if (bTracking)
    {
        FVector NewLocation(
            Z * PositionScale,
            X * PositionScale,
            -Y * PositionScale
        );
        FRotator NewRotation(Pitch, Yaw, Roll);
        SetActorLocationAndRotation(NewLocation, NewRotation);
    }
}

void ATrackerReceiver::EndPlay(const EEndPlayReason::Type Reason)
{
    if (ListenSocket)
    {
        ListenSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket);
        ListenSocket = nullptr;
    }
    Super::EndPlay(Reason);
}