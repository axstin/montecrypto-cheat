#define _CRT_SECURE_NO_WARNINGS

#include "main.h"

#include <Windows.h>

#include <string>
#include <iostream>
#include <vector>
#include <cctype>
#include <algorithm>
#include <regex>

#include "memutil.h"
#include "unrealapi.h"
#include "disassembler.h"

using namespace Unreal;

extern "C" void* __get_rsp();

void DllInit();
void DllExit();

void CreateConsole(const char* pszTitle);
void DestroyConsole();

std::vector<std::string> GetArguments(const std::string& input, const char delim = ' ')
{
	std::vector<std::string> rtn;
	int i = 0;
	int pos = -1;
	int sz = input.size();

	for (; i < sz; i++)
	{
		if (input[i] != delim && pos == -1)
		{
			pos = i;
			continue;
		}

		if (input[i] == delim && pos != -1)
		{
			rtn.push_back(input.substr(pos, i - pos));
			pos = -1;
		}
	}

	if (pos != -1)
		rtn.push_back(input.substr(pos, sz - pos));

	return rtn;
}

void EraseNonNumeric(std::string& input)
{
	input.erase(std::remove_if(input.begin(), input.end(), [](char c) { return !std::isdigit(c) && c != '.' && c != '-'; }), input.end());
}

/* Window Hook */

namespace Window
{
	bool KeysDown[256] = { false };
	HWND Handle = 0;
	WNDPROC OldWindowProc = 0;

	void OnKeyDown(unsigned char Key);
	void OnKeyUp(unsigned char Key);
	void Unhook();

	LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		//printf("Inside WindowProc (hwnd: %p, message: %X, WM_KEYDOWN: %X\n", hwnd, message, WM_KEYDOWN);

		switch (message)
		{
		case WM_KEYDOWN:
			if (wParam < 256)
			{
				OnKeyDown(wParam);
				KeysDown[wParam] = true;
			}
			break;
		case WM_KEYUP:
			if (wParam < 256)
			{
				OnKeyUp(wParam);
				KeysDown[wParam] = false;
			}
			break;
		case WM_CLOSE:
		case WM_DESTROY:
		case WM_QUIT:
			Unhook();
		}

		return CallWindowProc(OldWindowProc, hwnd, message, wParam, lParam);
	}

	bool Initiate()
	{
		// Enumerate all windows associated with the game process and find the first one with "CryptoChallenge" in the title
		BOOL bResult = EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL
		{
			DWORD processId;
			if (GetWindowThreadProcessId(hwnd, &processId) && processId == GetCurrentProcessId())
			{
				char title[256] = { 0 };
				GetWindowText(hwnd, title, sizeof(title));

				if (strstr(title, "CryptoChallenge"))
				{
					Handle = hwnd;
					return FALSE;
				}
			}

			return TRUE;
		}, NULL);

		if (!Handle)
			return false;

		LONG_PTR Result = SetWindowLongPtr(Handle, GWLP_WNDPROC, (LONG_PTR)WindowProc);
		if (!Result) printf("GetLastError(): %X\n", GetLastError());
		OldWindowProc = (WNDPROC)Result;

		return true;
	}

	void Unhook()
	{
		if (Handle) SetWindowLongPtr(Handle, GWLP_WNDPROC, (LONG_PTR)OldWindowProc);
	}
}


/* Main */

bool NoclipEnabled = false;

// Commented out bits were a few things I was experimenting with

void RequestVelocity(Vector Velocity)
{
	//LocalCharacter->Movement->RequestedVelocity = LocalCharacter->Movement->Velocity + Velocity;
	//LocalCharacter->Movement->VelocityFlags |= 0x400000;
	LocalCharacter->Movement->Velocity = LocalCharacter->Movement->Velocity + Velocity;
}

void StopVerticalMovement()
{
	//LocalCharacter->Movement->Acceleration.Z = 0;
	//LocalCharacter->Movement->RequestedVelocity.Z = 0;
	//LocalCharacter->Movement->VelocityFlags &= ~0x400000;
	LocalCharacter->Movement->Velocity.Z = 0;
	//(GetVirtualFunction<StopActiveMovementFn>(LocalCharacter->Movement, 171))(LocalCharacter->Movement);
	//(GetVirtualFunction<SetMovementModeFn>(LocalCharacter->Movement, 171))(LocalCharacter->Movement, 5, 0);
}

void ToggleNoclip()
{
	CharacterMovement* Movement = LocalCharacter->Movement;
	NoclipEnabled = !NoclipEnabled;

	// Attempt to replicate ACharacter::ClientCheatFly_Implementation (it is excluded in UE4 shipping builds)
	// https://github.com/EpicGames/UnrealEngine/blob/4.18/Engine/Source/Runtime/Engine/Private/Character.cpp#L1569

	if (NoclipEnabled)
	{
		LocalCharacter->CollisionFlags &= ~2; // Remove collision
		LocalCharacter->Movement->SetMovementMode(5, 0); // Set movement mode to MOVE_Flying
		StopVerticalMovement();
	}
	else
	{
		LocalCharacter->CollisionFlags |= 2; // Add collision
		LocalCharacter->Movement->SetMovementMode(1, 0); // Set movement mode to MOVE_Walking
	}
}

void Window::OnKeyDown(unsigned char Key)
{

}

FILE* BlueprintRecordFile = 0;

void Window::OnKeyUp(unsigned char Key)
{
	if (GetLocalCharacter())
	{
		if (Key == 0x4E) // N
		{
			ToggleNoclip();
		}
		else if (Key == VK_SPACE || Key == VK_LCONTROL || Key == VK_CONTROL)
		{
			if (NoclipEnabled) StopVerticalMovement();
		}
		else if (Key == 0x4A && !BlueprintRecordFile)
		{
			BlueprintRecordFile = fopen("record.txt", "w");
			printf("Recording started\n");
		}
		else if (Key == 0x4B && BlueprintRecordFile)
		{
			fclose(BlueprintRecordFile);
			BlueprintRecordFile = 0;
			printf("Stopped recording\n");
		}
	}
}

void CommandLoop()
{

	const char* const CHANGELOG = R"(
v4.0
+ Added 'sleep' command
+ Added argument for 'nametest' command

v3.5
+ Added 'nametest' command
+ Added 'debug' command

v3.0-hotfix-1
* Fixed anti-teleport bypass not working with CryptoChallenge Helper

v3.0
+ Added new seamless anti-teleport bypass. It doesn't require a command to be run, doesn't spoof your coordinates, and won't reset the game.
+ Added 'changelog' and 'exit' commands
- Removed 'bypassantitp' command

v2.5
+ Added an anti-teleport bypass by naPalm

v2.0
* Fixed hotkeys not working for some
* Fixed cheat breaking when the game reloads

v1.0
+ Initial Release
)";

	const char* const COMMANDS_AND_HOTKEYS = R"(
Commands:
noclip - Toggle noclip
coords/c - Prints your current location
teleport/tp X, Y, Z - Teleports to a location
changelog - View changelog
nametest FILENAME - Writes information about all objects in all loaded levels to FILENAME. If no argument is provided, actors.txt is used.
debug - Prints debug info
sleep TIME - Sleeps for TIME milliseconds
exit - Ejects dll and exits the cheat

Hotkeys:
N - Toggle noclip
Space - Fly up (Noclip Only)
Left Control - Fly down (Noclip Only)
)";

	printf("%s\n", COMMANDS_AND_HOTKEYS);

	while (1)
	{
		putchar('>');

		std::string input;
		std::getline(std::cin, input);
		std::vector<std::string> arguments = GetArguments(input);

		if (GetLocalCharacter() && arguments.size())
		{
			std::string command = arguments[0];
			arguments.erase(arguments.begin());

			size_t args_start = input.find(command) + command.size() + 1;
			std::string raw_arguments = args_start <= input.size() ? input.substr(args_start) : "";

			if (command == "exit")
			{
				break;
			}
			else if (command == "noclip")
			{
				ToggleNoclip();
			}
			else if (command == "coords" || command == "c")
			{
				Vector& Location = LocalCharacter->GetLocation();
				printf("Current Coordinates: %f, %f, %f\n", Location.X, Location.Y, Location.Z);
			}
			else if (command == "teleport" || command == "tp")
			{
				if (arguments.size() == 3)
				{
					// Remove any commas or etc. from the arguments
					EraseNonNumeric(arguments[0]);
					EraseNonNumeric(arguments[1]);
					EraseNonNumeric(arguments[2]);

					Vector destination;
					bool success = false;

					try
					{
						destination.X = std::stof(arguments[0]);
						destination.Y = std::stof(arguments[1]);
						destination.Z = std::stof(arguments[2]);
						success = true;
					}
					catch (std::exception& e)
					{
						printf("Usage:\ntp X, Y, Z\nOR\ntp X Y Z\n");
						success = false;
					}

					if (success)
					{
						printf("Teleporting to %f, %f, %f...\n", destination.X, destination.Y, destination.Z);
						LocalCharacter->TeleportTo(destination);
					}
				}
				else
				{
					printf("Usage:\ntp X, Y, Z\nOR\ntp X Y Z\n");
				}
			}
			else if (command == "changelog")
			{
				printf("%s", CHANGELOG);
			}
			else if (command == "debug")
			{
				printf("OldProc: %p\n", Window::OldWindowProc);
				printf("LocalPlayer: %p\n", LocalPlayer);
				printf("Attached Window: %p\n\n", Window::Handle);

				printf("Listing windows owned by this process:\n");
				BOOL bResult = EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL
				{
					DWORD processId;
					if (GetWindowThreadProcessId(hwnd, &processId) && processId == GetCurrentProcessId())
					{
						char title[256] = { 0 };
						GetWindowText(hwnd, title, sizeof(title));

						printf("%p: %s (equal to attached? %s)\n", hwnd, title, hwnd == Window::Handle ? "true" : "false");
					}

					return TRUE;
				}, NULL);
			}
			else if (command == "nametest")
			{	
				// UWorld::Levels "Array of levels currently in this world. Not serialized to disk to avoid hard references."
				// https://github.com/EpicGames/UnrealEngine/blob/4.18/Engine/Source/Runtime/Engine/Classes/Engine/World.h#L859
				TArray<byte_ptr>* levels = (TArray<byte_ptr>*)(GWorld + 0x110);

				std::string output_file = raw_arguments.size() > 0 ? raw_arguments : "actors.txt";
				FILE* file = fopen(output_file.c_str(), "w");
				
				if (file)
				{
					for (int i = 0; i < levels->Num; i++)
					{
						byte_ptr level = levels->Data[i];

						// ULevel::Actors "Array of all actors in this level, used by FActorIteratorBase and derived classes"
						// https://github.com/EpicGames/UnrealEngine/blob/4.18/Engine/Source/Runtime/Engine/Classes/Engine/Level.h#L359
						TArray<byte_ptr>* actors = (TArray<byte_ptr>*)(level + 0xA0);

						fprintf(file, "Level %d (Name: %s, ClassName: %s)\n", i, FName::GetObjectName(level).c_str(), FName::GetObjectClassName(level).c_str());
						fprintf(file, "Actor Count: %d\n\n", actors->Num);

						for (int j = 0; j < actors->Num; j++)
						{
							byte_ptr actor = actors->Data[j];
							if (actor)
							{
								fprintf(file, "%p: %s (%s), Location: ", actor, FName::GetObjectName(actor).c_str(), FName::GetObjectClassName(actor).c_str());

								AActor* casted_actor = (AActor*)actor;
								if (casted_actor->RootComponent)
								{
									Vector player_location = LocalCharacter->GetLocation();
									Vector location = casted_actor->GetLocation();
									fprintf(file, "%f, %f, %f (%f units away)\n", location.X, location.Y, location.Z, Vector::Distance2D(player_location, location));
								}
								else
								{
									fprintf(file, "N/A\n");
								}
							}
						}

						fprintf(file, "\n");
					}

					fprintf(file, "fin\n");
					printf("Wrote to %s\n", output_file.c_str());
					fclose(file);
				}
				else
				{
					printf("Unable to open file %s\n", output_file.c_str());
				}
			}
			else if (command == "materials")
			{
				// Ugly stuff I was testing

				if (arguments.size() > 0)
				{
					AActor* actor = (AActor*)strtoll(arguments[0].c_str(), nullptr, 16);
					printf("Actor: %p\n", actor);

					byte_ptr replacement_material = 0;
					if (arguments.size() > 1) replacement_material = (byte_ptr)strtoll(arguments[1].c_str(), nullptr, 16);

					try
					{
						typedef TArray<byte_ptr>*(*GetMaterialsFn)(void*, TArray<byte_ptr>*);
						typedef void(*SetMaterialFn)(void* self, int ElementIndex, void* Material);
						typedef byte_ptr(*CreateAndSetMaterialInstanceDynamicFn)(void*, int ElementIndex);

						byte_ptr component = actor->RootComponent;
						printf("Component: %p\n", component);

						TArray<byte_ptr> materials;
						(memutil->GetVirtualFunction<GetMaterialsFn>(component, 254))(component, &materials);

						printf("Count: %d\n", materials.Num);

						for (int i = 0; i < materials.Num; i++)
						{
							byte_ptr material = materials[i];

							printf("Material: %p (%s, %s)\n", material, FName::GetObjectName(material).c_str(), FName::GetObjectClassName(material).c_str());

							if (replacement_material)
							{
								printf("Replacing with %p\n", replacement_material);
								(memutil->GetVirtualFunction<SetMaterialFn>(component, 159))(component, i, replacement_material);
							}
							//(memutil->GetVirtualFunction<CreateAndSetMaterialInstanceDynamicFn>(component, 161))(component, i);

							*(int*)(material + 0x82C) |= 1;

							printf("wtf\n");
						}
					}
					catch (...)
					{
						printf("Invalid pointer\n");
					}
				}
			}
			else if (command == "sleep")
			{
				if (arguments.size() > 0)
				{
					int milliseconds;

					try
					{
						milliseconds = std::atoi(arguments[0].c_str());
					}
					catch (std::exception& e)
					{
						milliseconds = -1;
					}

					if (milliseconds != -1)
					{
						Sleep(milliseconds);
					}
					else
					{
						printf("Invalid argument\n");
					}
				}
			}
			else if (command == "goto")
			{
				// Forgot where I got these names/coordinates from but I created a script to parse them and create a C++ std::map for me

				static std::map<std::string, Vector> Places = {
					{ "spawn room",{ 24263.97f, 23992.59f, -220.0f } },
					{ "-> entrance compass",{ 23864.0f, 25020.0f, -277.0f } },
					{ "-> corridor compass 1",{ 1043.0f, 41529.0f, 122.0f } },
					{ "-> corridor compass 2",{ 21561.0f, 1282.0f, 122.0f } },
					{ "-> corridor compass 3",{ 18762.0f, 31058.0f, 122.0f } },
					{ "-> corridor compass 4",{ 43082.0f, 36318.0f, 122.0f } },
					{ "big room",{ 9512.0f, 5120.0f, 533.0f } },
					{ "1. epilepsy",{ 9339.0f, 5092.0f, 533.0f } },
					{ "-> epilepsy compass",{ 6514.0f, 5074.0f, 122.0f } },
					{ "2. outside",{ 15897.0f, 3140.0f, 533.0f } },
					{ " -> clickable candle",{ 12670.0f, -14750.0f, 2087.0f } },
					{ "outsidecandle",{ 12670.0f, -14750.0f, 2087.0f } },
					{ " -> clippable wall",{ 1591.0f, -14730.0f, 2988.0f } },
					{ " -> boulder #1 (entrance)",{ 14748.0f, 3515.0f, 1723.0f } },
					{ " -> boulder #2 (back left)",{ 8527.0f, -15678.0f, 1809.0f } },
					{ " -> boulder #3 (back right)",{ 17387.0f, -18143.0f, 2353.0f } },
					{ " -> boulder #4 (center left)",{ 8860.0f, -15852.0f, 1703.0f } },
					{ "3. white glass",{ 24221.0f, 4712.0f, 533.0f } },
					{ "-> white glass compass",{ 7373.0f, 24331.0f, 122.0f } },
					{ "4. loudsky",{ 35091.0f, 4184.0f, 533.0f } },
					{ "-> loudsky compass",{ 7399.0f, 31766.0f, 522.0f } },
					{ "5. fairy",{ 41630.0f, 4707.0f, 533.0f } },
					{ "6. desk",{ 6716.0f, 10818.0f, 533.0f } },
					{ "-> desk compass",{ 16933.0f, 6296.0f, 224.0f } },
					{ "7. wooden pits",{ 18201.0f, 13098.0f, 533.0f } },
					{ "-> pits compass",{ 15807.0f, 15820.0f, -375.0f } },
					{ "8. skull",{ 25507.0f, 11416.0f, 533.0f } },
					{ "9. four doors",{ 31334.0f, 13085.0f, 533.0f } },
					{ "10. parlor",{ 45477.0f, 12682.0f, 533.0f } },
					{ "-> parlor compass",{ 14899.0f, 43828.0f, 1322.0f } },
					{ "-> horse statues",{ 3199.0f, -3298.0f, -811.0f } },
					{ "11. rain",{ 3464.0f, 24991.0f, 533.0f } },
					{ "-> rain temple",{ 6243.0f, 50172.0f, 24395.0f } },
					{ "12. goblin",{ 18290.0f, 23647.0f, 533.0f } },
					{ "13. frog",{ 29933.0f, 25076.0f, 533.0f } },
					{ "14. server",{ 35701.0f, 21093.0f, 533.0f } },
					{ "-> server compass",{ 24894.0f, 43418.0f, -820.0f } },
					{ "15. robot",{ 8739.0f, 35485.0f, 533.0f } },
					{ "16. harry potter",{ 15111.0f, 36116.0f, 533.0f } },
					{ "-> harry potter compass",{ 34360.0f, 14715.0f, 122.0f } },
					{ "17. pi",{ 25538.0f, 29466.0f, 533.0f } },
					{ "-> pi room (alt)",{ 25503.0f, 29877.0f, 533.0f } },
					{ "-> pi compass",{ 32982.0f, 24914.0f, 3022.0f } },
					{ "18. blue",{ 33995.0f, 35509.0f, 533.0f } },
					{ "19. spirited machines",{ 38709.0f, 40963.0f, 533.0f } },
					{ "20. meditation",{ 5114.0f, 44803.0f, 533.0f } },
					{ "-> tiny sofa compass",{ 43114.0f, 6275.0f, 122.0f } },
					{ "21. routine",{ 15908.0f, 45168.0f, 533.0f } },
					{ "-> routine compass",{ 41812.0f, 14973.0f, 124.0f } },
					{ "22. binary",{ 22703.0f, 44893.0f, 533.0f } },
					{ "-> binary compass",{ 43426.0f, 24534.0f, 122.0f } },
					{ "23. bedroom",{ 32280.0f, 45364.0f, 533.0f } },
					{ "24. lake-wave",{ 43532.0f, 44709.0f, 533.0f } },
					{ "-> lake-wave compass",{ 42152.0f, 41998.0f, -120.0f } },
					{ "yellow trail",{ 24376.0f, 28697.0f, 533.0f } },
					{ "blue line spawn room",{ 28368.0f, 24634.0f, 533.0f } }
				};

				for (auto& place : Places)
				{
					if (place.first.find(raw_arguments) != std::string::npos)
					{
						LocalCharacter->TeleportTo(place.second);
						break;
					}
				}
			}
			else if (command == "disasm")
			{
				UFunction* function = (UFunction*)strtoll(arguments[0].c_str(), nullptr, 16);

				FKismetBytecodeDisassembler disassembler(stdout);
				disassembler.DisassembleStructure(function);
			}
			else if (command == "name")
			{
				if (arguments.size() > 0)
				{
					UObject* object = (UObject*)strtoll(arguments[0].c_str(), nullptr, 16);
					printf("%s (is an actor? %d)\n-\n", object->GetName().c_str(), object->IsA("Actor"));
					
					object = object->GetClass();

					while (object)
					{
						printf("%s\n", object->GetName().c_str());
						object = object->GetSuper();
					}
				}
			}
			else if (command == "search")
			{
				for (int i = 0; i < GUObjectArray->Num && arguments.size() > 0; i++)
				{
					FUObjectItem& Item = GUObjectArray->Objects[i];

					if (Item.Object && Item.Object->GetName().find(arguments[0]) != std::string::npos)
					{
						printf("%p: %s (Is Struct? %d)\n", Item.Object, Item.Object->GetName().c_str(), Item.Object->IsA("Struct"));

						if (Item.Object->IsA("Struct"))
						{
							printf("Script Num: %d\n", ((UFunction*)Item.Object)->Script.Num);
							printf("Native: %d\n", ((UFunction*)Item.Object)->Flags & FUNC_Native != 0);
						}
					}
				}
			}
			else if (command == "dumpfuncs")
			{
				for (int i = 0; i < GUObjectArray->Num; i++)
				{
					FUObjectItem& Item = GUObjectArray->Objects[i];

					if (Item.Object && Item.Object->IsA("Struct")) // Objects that are a "Struct" contain bytecode: https://github.com/EpicGames/UnrealEngine/blob/4.18/Engine/Source/Runtime/CoreUObject/Public/UObject/Class.h#L236
					{
						// Assume the object is a UFunction (might be dangerous? seems to work fine for this use case, though)
						UFunction* Function = (UFunction*)Item.Object;

						if (!(Function->Flags & FUNC_Native))
						{
							std::string FunctionName = Function->GetName();
							UObject* Parent = Function->GetParent();
							std::string ParentName = Parent->GetName();
							std::string ParentClass = Parent->GetClass()->GetName();

							// Is there any bytecode in this object?
							if (Function->Script.Num > 0)
							{
								printf("Dumping... %p: %s %d (Parent: %s)\n", Function, FunctionName.c_str(), Function->Script.Num, ParentName.c_str());

								std::string FileName = "Disassembly/";
								FileName += std::regex_replace(ParentName, std::regex("/"), "__") + "." + FunctionName + ".txt";

								FILE* File = fopen(FileName.c_str(), "w");
								if (File)
								{
									fprintf(File, "Dumped using ue4cheat.dll by Austin\n\nName: %s\nOwner: %s\nOwner Class: %s\n\n", FunctionName.c_str(), ParentName.c_str(), ParentClass.c_str());

									/*if (Parent->IsA("Actor"))
									{
										AActor* Actor = (AActor*)Parent;
										if (Actor->RootComponent)
										{
											Vector Location = Actor->GetLocation();
											fprintf(File, "Owner Location: %f, %f, %f\n", Location.X, Location.Y, Location.Z);
										}
										else fprintf(File, "Owner Location: N/A\n");
									}
									else fprintf(File, "Owner Location: N/A\n");*/

									fprintf(File, "Script Object References: %d\n", Function->ScriptObjectReferences.Num);
									for (int j = 0; j < Function->ScriptObjectReferences.Num; j++)
									{
										UObject* Reference = Function->ScriptObjectReferences.Data[j];
										if (Reference)
										{
											fprintf(File, "\t- %p: %s (%s)\n", Reference, Reference->GetName().c_str(), Reference->GetClass()->GetName().c_str());
										}
										else
										{
											fprintf(File, "\t- NULL\n");
										}
									}

									fprintf(File, "\nBytecode Size: %d\n\n", Function->Script.Num);
									
									// Disassemble it!
									FKismetBytecodeDisassembler Disassembler(File);
									Disassembler.DisassembleStructure(Function);
	
									fclose(File);
								}
								else
								{
									printf("ERROR: Unable to open %s\n", FileName.c_str());
									break;
								}
							}
						}
						else
						{
							printf("ok\n");
						}
					}
				}
			}
			else if (command == "bp")
			{
				printf("Hook written (don't run again) J = Start recording to record.txt, K = Stop recording\n");
				
				// Here are various C functions I hooked to test logging blueprint/kismet function calls in the VM

				typedef void(*CallFunctionFn)(void* self, uintptr_t stack, uintptr_t unknown, void* function);
				static CallFunctionFn CallFunction = (CallFunctionFn)memutil->offset(0x42AF60);
				static CallFunctionFn oCallFunction;

				// https://github.com/EpicGames/UnrealEngine/blob/4.18/Engine/Source/Runtime/CoreUObject/Private/UObject/ScriptCore.cpp#L670
				oCallFunction = memutil->CreateDetour<CallFunctionFn>(CallFunction, [](void* self, uintptr_t stack, uintptr_t unknown, void* function) -> void
				{
					if (function && BlueprintRecordFile)
					{
						std::string name = FName::GetObjectName(self);
						std::string class_name = FName::GetObjectClassName(self);
						/*if (//class_name != "CharacterMovementComponent" &&
							//class_name != "CameraComponent" &&
							class_name != "BP_Painting_C" &&
							class_name != "KismetStringLibrary" &&
							
							//name.find("Default__") == std::string::npos &&
							name != "TextMessage1" &&
							name != "TextDate" &&
							name != "Timeline_0" &&
							name != "Canvas_0" &&
							name != "BP_Map_64" &&
							name.find("Timer") == std::string::npos)
						{*/
						bool native = (*(uint32_t*)((byte_ptr)function + 136) & 0x400) != 0;
						fprintf(BlueprintRecordFile, "[CF] %p (%s, %s): F: %p, N: %s, P: %p, Native: %d\n", self, name.c_str(), class_name.c_str(), stack, FName::GetObjectName(function).c_str(), *(void**)((byte_ptr)function + 176), native);
						//fflush(BlueprintRecordFile);						
						//}
					}

					oCallFunction(self, stack, unknown, function);
				}, 0);

				typedef void(*ProcessEventFn)(void*, void*, void*);
				static ProcessEventFn oProcessEvent;

				// https://github.com/EpicGames/UnrealEngine/blob/4.18/Engine/Source/Runtime/CoreUObject/Private/UObject/ScriptCore.cpp#L1156
				oProcessEvent = memutil->CreateDetour<ProcessEventFn>(memutil->offset(0x432150), [](void* self, void* function, void* parms) -> void
				{
					if (function && BlueprintRecordFile)
					{
						std::string name = FName::GetObjectName(self);
						std::string class_name = FName::GetObjectClassName(self);
						fprintf(BlueprintRecordFile, "[PE] %p (%s, %s): N: %s\n", self, name.c_str(), class_name.c_str(), FName::GetObjectName(function).c_str());
					}

					oProcessEvent(self, function, parms);
				}, 1);

				typedef void(*FuncInvokeFn)(void*, void* Obj, void* Stack, void*);
				static FuncInvokeFn oFuncInvoke;

				// https://github.com/EpicGames/UnrealEngine/blob/4.18/Engine/Source/Runtime/CoreUObject/Private/UObject/Class.cpp#L4531
				oFuncInvoke = memutil->CreateDetour<FuncInvokeFn>(memutil->offset(0x2E27D0), [](void* self, void* object, void* stack, void* unk) -> void
				{
					if (BlueprintRecordFile)
					{
						std::string name = FName::GetObjectName(object);
						std::string class_name = FName::GetObjectClassName(object);
						fprintf(BlueprintRecordFile, "[FI] %p (%s, %s): N: %s, P: %p\n", object, name.c_str(), class_name.c_str(), FName::GetObjectName(self).c_str(), *(void**)((byte_ptr)self + 176));
					}

					oFuncInvoke(self, object, stack, unk);
				}, 0);

				typedef void(*ProcessInternalFn)(void* self, byte* stack, void*);
				static ProcessInternalFn oProcessInternal;

				// https://github.com/EpicGames/UnrealEngine/blob/4.18/Engine/Source/Runtime/CoreUObject/Private/UObject/ScriptCore.cpp#L887
				oProcessInternal = memutil->CreateDetour<ProcessInternalFn>(memutil->offset(0x4324C0), [](void* self, byte* stack, void* unk) -> void
				{
					if (BlueprintRecordFile)
					{
						byte* function = *(byte**)(stack + 16);
						std::string function_name = FName::GetObjectName(function);

						byte* start = *(byte**)(stack + 32);
						oProcessInternal(self, stack, unk);
						byte* end = *(byte**)(stack + 32);

						fprintf(BlueprintRecordFile, "[PI] %p (%s, %s): N: %s, Function: %p, Bytes Processed: %d\n", self, FName::GetObjectName(self).c_str(), FName::GetObjectClassName(self).c_str(), function_name.c_str(), function, (int)(end - start));

						/*if (function_name != "ExecuteUbergraph_BP_Character")
						{
							while (start++ <= end)
							{
								const char* opcode_name;
								try
								{
									opcode_name = EOpcodeToString.at((EOpcode)*start);
								}
								catch (...)
								{
									opcode_name = "???";
								}
								fprintf(BlueprintRecordFile, "\t- 0x%X (%s)\n", *start, opcode_name);
							}
						}*/
					}
					else
					{
						oProcessInternal(self, stack, unk);
					}
				}, 0);
			}
		}

		putchar('\n');
	}
}

// A thread spawned in DllInit() responsible for handling noclip
void MainThread()
{
	while (1)
	{
		if (GetLocalCharacter())
		{
			if (NoclipEnabled)
			{
				if (Window::KeysDown[VK_SPACE])
				{
					RequestVelocity({ 0, 0, 10 });
				}
				else if (Window::KeysDown[VK_LCONTROL] || Window::KeysDown[VK_CONTROL])
				{
					RequestVelocity({ 0, 0, -10 });
				}
			}
		}

		Sleep(10);
	}
}

typedef bool(*Greater_FloatFloatFn)(void*, byte*, bool*);
Greater_FloatFloatFn oGreater_FloatFloat;
Vector LastLocation = { 0,0,0 };

// The game's anti-teleport check was created using Unreal Engine 4 blueprints (much like almost all other mechanics in the game-- I don't think the developers wrote anything in C++)
// The check works by comparing your current position with a previous position every few seconds
// If the check detects you've traveled more than 1000 units between one of these intervals, the game resets
// This bypass works by hooking a function that the blueprint calls: Greater_FloatFloat ( https://github.com/EpicGames/UnrealEngine/blob/4.18/Engine/Source/Runtime/Engine/Classes/Kismet/KismetMathLibrary.inl#L351 )

bool Greater_FloatFloatHook(void* rcx, byte* data, bool* out)
{
	bool result = oGreater_FloatFloat(rcx, data, out);

	// Really really nasty way of getting the float values that were compared
	// Explanation: After a call to Greater_FloatFloat, the blueprint arguments can be found on the stack at [rsp + 0x8] and [rsp + 0x10] (this doesn't apply to all blueprint native/C function calls, of course)
	byte* stack = (byte*)__get_rsp();
	float a = *(float*)(stack + 0x8), b = *(float*)(stack + 0x10);

	if (GetLocalCharacter())
	{
		if (b == 1000) // most likely the anti-tp check ...
		{
			Vector location = LocalCharacter->GetLocation();

			// Additional check to prevent false positives (for example, if the game was comparing a float to 1000 in an area unrelated to the anti-tp check)
			if (Vector::Distance2D(LastLocation, location) >= 1000.0 && result)
			{
				printf("Anti-TP (almost) detected teleport (%f > %f == %d), bypassing...\n", a, b, result);
				*out = false;
				result = false;
			}

			LastLocation = location;
		}
	}

	return result;
}

void DllInit()
{
	CreateConsole("Montecrypto Cheat v4.0 by Austin");
	
	// Responsible for finding the game window and adding a hook so we can listen for hotkeys later (used for noclip, etc.)
	while (!Window::Initiate())
		Sleep(100);

	// Initiate Unreal API for interfacing with the game
	Unreal::Init();

	// Wait for the World, LocalPlayer, and Character to exist
	printf("Waiting for World and Character...\n");
	while (GetLocalCharacter() == 0)
		Sleep(500);

	printf("GWorld: %p\n", GWorld);
	printf("Character: %p\n", LocalCharacter);

	// Write Greater_FloatFloat detour/hook for bypassing the game's anti-teleport
	if ((oGreater_FloatFloat = (Greater_FloatFloatFn)memutil->CreateDetour(memutil->offset(ADDR_GREATER_FLOATFLOAT), Greater_FloatFloatHook, 0)) == 0)
		printf("ERROR: Unable to bypass Anti-Teleport\n");

	HANDLE Thread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, 0, 0, 0);
	CommandLoop();
	Window::Unhook();
	TerminateThread(Thread, 0);
	
	DllExit();
}

void DllExit()
{
	memutil->Clean();
	DestroyConsole();
	FreeLibraryAndExitThread(GetModuleHandleA("ue4cheat.dll"), 0);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hinstDLL);
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)DllInit, 0, 0, 0);
	}

	return TRUE;
}

void CreateConsole(const char* pszTitle)
{
	if (GetConsoleWindow()) return;

	AllocConsole();
	freopen("CONOUT$", "w", stdout);
	freopen("CONIN$", "r", stdin);
	freopen("CONERR$", "w", stderr);
	
	SetConsoleTitleA(pszTitle);
}

void DestroyConsole()
{
	if (!GetConsoleWindow()) return;

	ShowWindow(GetConsoleWindow(), SW_HIDE);
	FreeConsole();
}

