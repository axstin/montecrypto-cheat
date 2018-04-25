#include "unrealapi.h"
#include "memutil.h"
#include "main.h"

uintptr_t Address_GWorldPtr = ADDR_GWORLD;
uintptr_t Address_FNameToString = ADDR_FNAME_TOSTRING;

namespace Unreal
{
	std::string FName::ToString()
	{
		FString buffer;

		typedef void(*Fn)(FName*, FString*);
		((Fn)Address_FNameToString)(this, &buffer);

		// Ew
		std::wstring wresult = buffer.Data;
		return std::string(wresult.begin(), wresult.end());
	}

	std::string FName::GetObjectName(void* object)
	{
		return GetNameSafe(object);
	}

	std::string FName::GetObjectClassName(void* object)
	{
		return GetNameSafe(((UObject*)object)->GetClass());
	}

	std::string GetNameSafe(void* object)
	{
		if (object)
		{
			return ((UObject*)object)->GetName();
		}

		return "N/A";
	}

	void CharacterMovement::SetMovementMode(int mode, unsigned char unknown)
	{
		// https://github.com/EpicGames/UnrealEngine/blob/4.18/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp#L826

		typedef void(__fastcall *Fn)(void*, int, unsigned char);
		(memutil->GetVirtualFunction<Fn>(this, 171))(this, mode, unknown);
	}

	void AActor::TeleportTo(const Vector& DestLocation, const Rotator& DestRotation, bool bIsATest, bool bNoCheck)
	{
		// https://github.com/EpicGames/UnrealEngine/blob/4.18/Engine/Source/Runtime/Engine/Classes/GameFramework/Actor.h#L2286
		// https://github.com/EpicGames/UnrealEngine/blob/4.18/Engine/Source/Runtime/Engine/Private/Actor.cpp#L348

		typedef void(__fastcall *Fn)(void*, const Vector&, const Rotator&, bool, bool);
		(memutil->GetVirtualFunction<Fn>(this, 148))(this, DestLocation, DestRotation, bIsATest, bNoCheck);
	}

	void Update()
	{
		try
		{
			GWorld = *(byte_ptr*)(Address_GWorldPtr);
			LocalPlayer = **(ULocalPlayer***)(*(byte_ptr*)(GWorld + 0x140) + 0x38); // GWorld->OwningGameInstance->LocalPlayers[0] ( https://github.com/EpicGames/UnrealEngine/blob/4.18/Engine/Source/Runtime/Engine/Classes/Engine/GameInstance.h#L129 )
			LocalCharacter = LocalPlayer->GetLocalCharacter();
		}
		catch (...)
		{
			GWorld = 0;
			LocalPlayer = 0;
			LocalCharacter = 0;
		}
	}

	AActor* GetLocalCharacter()
	{
		Update();
		return LocalCharacter;
	}

	// Adjust constants and scan for any needed signatures
	void Init()
	{
		memutil->register_offset(&Address_GWorldPtr);
		memutil->register_offset(&Address_FNameToString);

		// Thanks to n. in the Montecrypto Solvers community for telling me about the existance of this array and how to find it
		SignatureScanner scanner;
		scanner.Add("GUObjectArray", SSFLAG_NOWARNING | SSFLAG_DEREFOFFSET, &GUObjectArray, "\x48\x63\xC8\x48\x8D\x14\x49\x48\x8B\x0D\x00\x00\x00\x00", "xxxxxxxxxx????", 10);
		scanner.Scan("CryptoChallenge-Win64-Shipping.exe");

		if (scanner.GetErrorCount() > 0)
		{
			printf("ERROR: Unable to find all signatures!\n");
			system("pause");
		}
	}

	FFixedUObjectArray* GUObjectArray = 0;
	byte_ptr GWorld = 0;
	ULocalPlayer* LocalPlayer = 0;
	AActor* LocalCharacter = 0;
}
