#pragma once

#include <map>
#include <string>
#include <codecvt>

#define UNREAL_WITH_CASE_PRESERVING_NAME 0
#define UNREAL_UNICODE 1 // Should be 1 for almost any UE4 game

#if UNREAL_UNICODE
#define UNREAL_TCHAR wchar_t
#else
#define UNREAL_TCHAR char
#endif

typedef unsigned char* byte_ptr;

namespace Unreal
{
	/* Structures */

	struct Vector
	{
		float X, Y, Z;

		Vector operator+(Vector& other) const
		{
			return { X + other.X, Y + other.Y, Z + other.Z };
		}

		static float Distance2D(Vector& a, Vector& b)
		{
			return sqrtf((b.X - a.X) * (b.X - a.X) + (b.Y - a.Y) * (b.Y - a.Y));
		}
	};
	static const Vector NullVector = { 0, 0, 0 };

	struct Rotator
	{
		float Pitch, Yaw, Roll;
	};

	template <typename T>
	struct TArray
	{
		T* Data;
		int Num;
		int Max;

		T& operator[](int32_t Index) const
		{
			return Data[Index];
		}
	};

	typedef TArray<UNREAL_TCHAR> FString;
	
	struct FScriptName /* For serialization */
	{
		int32_t ComparisonIndex;
		int32_t DisplayIndex; /* Used when WITH_CASE_PRESERVING_NAME is true */
		uint32_t Number;
	};

	struct FName
	{
		int32_t ComparisonIndex;
#if UNREAL_WITH_CASE_PRESERVING_NAME
		int32_t DisplayIndex;
#endif
		uint32_t Number;

		FName(const FScriptName& ScriptName)
			: ComparisonIndex(ScriptName.ComparisonIndex),
#if UNREAL_WITH_CASE_PRESERVING_NAME
			DisplayIndex(ScriptName.DisplayIndex),
#endif
			Number(ScriptName.Number)
		{
		}

		// TODO: These 2 functions are deprecated, remove
		static std::string GetObjectName(void* object);
		static std::string GetObjectClassName(void* object);
		std::string ToString();
	};

	struct UObject;

	/* NOTE: Be careful of FName size changes */
	struct UObjectInternal
	{
		unsigned char pad_0[8];      // +00  00
		uint32_t ObjectFlags;        // +08  08
		uint32_t InternalIndex;      // +0C  12
		UObject* ClassPrivate;       // +10  16
		FName NamePrivate;           // +18  24
		UObject* OuterPrivate;       // +20  32
		/* UField */
		UObject* Next;               // +28  40
		/* UStruct */
		UObject* SuperStruct;        // +30  48
	};

	struct UObject
	{
		UObjectInternal* Internal()
		{
			return (UObjectInternal*)this;
		}

		std::string GetName()
		{
			return Internal()->NamePrivate.ToString();
		}

		UObject* GetClass()
		{
			return Internal()->ClassPrivate;
		}

		UObject* GetParent()
		{
			return Internal()->OuterPrivate;
		}

		/* NOTE: Returns null for non-class objects? */
		UObject* GetSuper()
		{
			return Internal()->SuperStruct;
		}

		/* class_object should be a class (object->GetClass()) */
		static bool IsA(UObject* class_object, const std::string& what)
		{
			if (class_object == nullptr) return false;
			std::string class_name = class_object->GetName();
			if (class_name == what) return true;
			return IsA(class_object->GetSuper(), what);
		}

		bool IsA(const std::string& what)
		{
			return IsA(GetClass(), what);
		}
	};

	struct FUObjectItem
	{
		UObject* Object;
		uint32_t Flags;
		unsigned char Unknown[12];
	};

	/* like TArray but Num and Max are switched ??? */
	struct FFixedUObjectArray
	{
		FUObjectItem* Objects;
		uint32_t Max;
		uint32_t Num;

		FUObjectItem& operator[](int32_t Index) const
		{
			return Objects[Index];
		}
	};
	
	struct CharacterMovement : UObject
	{
		unsigned char pad_0[260];    // +00  00
		Vector Velocity;             // +104  260
		unsigned char pad_1[376];    // +110  272
		Vector Acceleration;         // +288  648
		unsigned char pad_2[304];    // +294  660
		int VelocityFlags;           // +3C4  964 | 0x400000 = bHasRequestedVelocity
		unsigned char pad_3[20];     // +3C8  968
		Vector RequestedVelocity;    // +3DC  988

		void SetMovementMode(int mode, unsigned char unknown = 0);
	};

	struct AActor : UObject
	{
		unsigned char pad_0[134];    // +00  00
		unsigned char CollisionFlags;// +86  134 | 0x2  = collision
		unsigned char pad_1[217];    // +87  135
		byte_ptr RootComponent;      // +160  352
		unsigned char pad_2[616];    // +168  360
		CharacterMovement* Movement; // +3D0  976

		Vector& GetLocation()
		{
			return *(Vector*)(RootComponent + 0x190);
		}

		Rotator& GetRotation()
		{
			return *(Rotator*)(RootComponent + 0x180);
		}

		void TeleportTo(Vector location)
		{
			TeleportTo(location, GetRotation(), false, true);
		}

		void TeleportTo(const Vector& DestLocation, const Rotator& DestRotation, bool bIsATest, bool bNoCheck);
	};

	struct ULocalPlayer : UObject
	{
		unsigned char pad_0[112];    // +00  00
		Vector Location;             // +70  112

		AActor* GetLocalCharacter()
		{
			return *(AActor**)(*(byte_ptr*)((byte_ptr)this + 0x30) + 0x380); // LocalPlayer->PlayerController->AcknowledgedPawn ?
		}
	};

	/* Globals */
	extern FFixedUObjectArray* GUObjectArray; // List of all objects in the game (thanks n.)
	extern byte_ptr GWorld; // World object
	extern ULocalPlayer* LocalPlayer;
	extern AActor* LocalCharacter;

	/* API */
	
	void Update(); // Update globals (GWorld, LocalPlayer, LocalCharacter, etc.)
	AActor* GetLocalCharacter(); // Update globals and return LocalCharacter
	void Init(); // Init API
	std::string GetNameSafe(void* object);

	/* Util */

	inline std::wstring StringToWString(const std::string& str)
	{
		typedef std::codecvt_utf8<wchar_t> convert_typeX;
		std::wstring_convert<convert_typeX, wchar_t> converterX;

		return converterX.from_bytes(str);
	}

	inline std::string WStringToString(const std::wstring& wstr)
	{
		typedef std::codecvt_utf8<wchar_t> convert_typeX;
		std::wstring_convert<convert_typeX, wchar_t> converterX;

		return converterX.to_bytes(wstr);
	}

	/* Internals */

	enum EExprToken
	{
		// Variable references.
		EX_LocalVariable = 0x00,	// A local variable.
		EX_InstanceVariable = 0x01,	// An object variable.
		EX_DefaultVariable = 0x02, // Default variable for a class context.
		//						= 0x03,
		EX_Return = 0x04,	// Return from function.
		//						= 0x05,
		EX_Jump = 0x06,	// Goto a local address in code.
		EX_JumpIfNot = 0x07,	// Goto if not expression.
		//						= 0x08,
		EX_Assert = 0x09,	// Assertion.
		//						= 0x0A,
		EX_Nothing = 0x0B,	// No operation.
		//						= 0x0C,
		//						= 0x0D,
		//						= 0x0E,
		EX_Let = 0x0F,	// Assign an arbitrary size value to a variable.
		//						= 0x10,
		//						= 0x11,
		EX_ClassContext = 0x12,	// Class default object context.
		EX_MetaCast = 0x13, // Metaclass cast.
		EX_LetBool = 0x14, // Let boolean variable.
		EX_EndParmValue = 0x15,	// end of default value for optional function parameter
		EX_EndFunctionParms = 0x16,	// End of function call parameters.
		EX_Self = 0x17,	// Self object.
		EX_Skip = 0x18,	// Skippable expression.
		EX_Context = 0x19,	// Call a function through an object context.
		EX_Context_FailSilent = 0x1A, // Call a function through an object context (can fail silently if the context is NULL; only generated for functions that don't have output or return values).
		EX_VirtualFunction = 0x1B,	// A function call with parameters.
		EX_FinalFunction = 0x1C,	// A prebound function call with parameters.
		EX_IntConst = 0x1D,	// Int constant.
		EX_FloatConst = 0x1E,	// Floating point constant.
		EX_StringConst = 0x1F,	// String constant.
		EX_ObjectConst = 0x20,	// An object constant.
		EX_NameConst = 0x21,	// A name constant.
		EX_RotationConst = 0x22,	// A rotation constant.
		EX_VectorConst = 0x23,	// A vector constant.
		EX_ByteConst = 0x24,	// A byte constant.
		EX_IntZero = 0x25,	// Zero.
		EX_IntOne = 0x26,	// One.
		EX_True = 0x27,	// Bool True.
		EX_False = 0x28,	// Bool False.
		EX_TextConst = 0x29, // FText constant
		EX_NoObject = 0x2A,	// NoObject.
		EX_TransformConst = 0x2B, // A transform constant
		EX_IntConstByte = 0x2C,	// Int constant that requires 1 byte.
		EX_NoInterface = 0x2D, // A null interface (similar to EX_NoObject, but for interfaces)
		EX_DynamicCast = 0x2E,	// Safe dynamic class casting.
		EX_StructConst = 0x2F, // An arbitrary UStruct constant
		EX_EndStructConst = 0x30, // End of UStruct constant
		EX_SetArray = 0x31, // Set the value of arbitrary array
		EX_EndArray = 0x32,
		//						= 0x33,
		EX_UnicodeStringConst = 0x34, // Unicode string constant.
		EX_Int64Const = 0x35,	// 64-bit integer constant.
		EX_UInt64Const = 0x36,	// 64-bit unsigned integer constant.
		//						= 0x37,
		EX_PrimitiveCast = 0x38,	// A casting operator for primitives which reads the type as the subsequent byte
		EX_SetSet = 0x39,
		EX_EndSet = 0x3A,
		EX_SetMap = 0x3B,
		EX_EndMap = 0x3C,
		EX_SetConst = 0x3D,
		EX_EndSetConst = 0x3E,
		EX_MapConst = 0x3F,
		EX_EndMapConst = 0x40,
		//						= 0x41,
		EX_StructMemberContext = 0x42, // Context expression to address a property within a struct
		EX_LetMulticastDelegate = 0x43, // Assignment to a multi-cast delegate
		EX_LetDelegate = 0x44, // Assignment to a delegate
		//						= 0x45, 
		//						= 0x46, // CST_ObjectToInterface
		//						= 0x47, // CST_ObjectToBool
		EX_LocalOutVariable = 0x48, // local out (pass by reference) function parameter
		//						= 0x49, // CST_InterfaceToBool
		EX_DeprecatedOp4A = 0x4A,
		EX_InstanceDelegate = 0x4B,	// const reference to a delegate or normal function object
		EX_PushExecutionFlow = 0x4C, // push an address on to the execution flow stack for future execution when a EX_PopExecutionFlow is executed.   Execution continues on normally and doesn't change to the pushed address.
		EX_PopExecutionFlow = 0x4D, // continue execution at the last address previously pushed onto the execution flow stack.
		EX_ComputedJump = 0x4E,	// Goto a local address in code, specified by an integer value.
		EX_PopExecutionFlowIfNot = 0x4F, // continue execution at the last address previously pushed onto the execution flow stack, if the condition is not true.
		EX_Breakpoint = 0x50, // Breakpoint.  Only observed in the editor, otherwise it behaves like EX_Nothing.
		EX_InterfaceContext = 0x51,	// Call a function through a native interface variable
		EX_ObjToInterfaceCast = 0x52,	// Converting an object reference to native interface variable
		EX_EndOfScript = 0x53, // Last byte in script code
		EX_CrossInterfaceCast = 0x54, // ConveFunctionrting an interface variable reference to native interface variable
		EX_InterfaceToObjCast = 0x55, // Converting an interface variable reference to an object
		//						= 0x56,
		//						= 0x57,
		//						= 0x58,
		//						= 0x59,
		EX_WireTracepoint = 0x5A, // Trace point.  Only observed in the editor, otherwise it behaves like EX_Nothing.
		EX_SkipOffsetConst = 0x5B, // A CodeSizeSkipOffset constant
		EX_AddMulticastDelegate = 0x5C, // Adds a delegate to a multicast delegate's targets
		EX_ClearMulticastDelegate = 0x5D, // Clears all delegates in a multicast target
		EX_Tracepoint = 0x5E, // Trace point.  Only observed in the editor, otherwise it behaves like EX_Nothing.
		EX_LetObj = 0x5F,	// assign to any object ref pointer
		EX_LetWeakObjPtr = 0x60, // assign to a weak object pointer
		EX_BindDelegate = 0x61, // bind object and name to delegate
		EX_RemoveMulticastDelegate = 0x62, // Remove a delegate from a multicast delegate's targets
		EX_CallMulticastDelegate = 0x63, // Call multicast delegate
		EX_LetValueOnPersistentFrame = 0x64,
		EX_ArrayConst = 0x65,
		EX_EndArrayConst = 0x66,
		EX_SoftObjectConst = 0x67,
		EX_CallMath = 0x68, // static pure function from on local call space
		EX_SwitchValue = 0x69,
		EX_InstrumentationEvent = 0x6A, // Instrumentation event
		EX_ArrayGetByRef = 0x6B,
		EX_Max = 0x100,
	};

	typedef EExprToken EOpcode;

	static const std::map<EOpcode, const char*> EOpcodeToString = {
		{ EX_LocalVariable, "EX_LocalVariable" },
		{ EX_InstanceVariable, "EX_InstanceVariable" },
		{ EX_DefaultVariable, "EX_DefaultVariable" },
		{ EX_Return, "EX_Return" },
		{ EX_Jump, "EX_Jump" },
		{ EX_JumpIfNot, "EX_JumpIfNot" },
		{ EX_Assert, "EX_Assert" },
		{ EX_Nothing, "EX_Nothing" },
		{ EX_Let, "EX_Let" },
		{ EX_ClassContext, "EX_ClassContext" },
		{ EX_MetaCast, "EX_MetaCast" },
		{ EX_LetBool, "EX_LetBool" },
		{ EX_EndParmValue, "EX_EndParmValue" },
		{ EX_EndFunctionParms, "EX_EndFunctionParms" },
		{ EX_Self, "EX_Self" },
		{ EX_Skip, "EX_Skip" },
		{ EX_Context, "EX_Context" },
		{ EX_Context, "EX_Context" },
		{ EX_VirtualFunction, "EX_VirtualFunction" },
		{ EX_FinalFunction, "EX_FinalFunction" },
		{ EX_IntConst, "EX_IntConst" },
		{ EX_FloatConst, "EX_FloatConst" },
		{ EX_StringConst, "EX_StringConst" },
		{ EX_ObjectConst, "EX_ObjectConst" },
		{ EX_NameConst, "EX_NameConst" },
		{ EX_RotationConst, "EX_RotationConst" },
		{ EX_VectorConst, "EX_VectorConst" },
		{ EX_ByteConst, "EX_ByteConst" },
		{ EX_IntZero, "EX_IntZero" },
		{ EX_IntOne, "EX_IntOne" },
		{ EX_True, "EX_True" },
		{ EX_False, "EX_False" },
		{ EX_TextConst, "EX_TextConst" },
		{ EX_NoObject, "EX_NoObject" },
		{ EX_TransformConst, "EX_TransformConst" },
		{ EX_IntConstByte, "EX_IntConstByte" },
		{ EX_NoInterface, "EX_NoInterface" },
		{ EX_DynamicCast, "EX_DynamicCast" },
		{ EX_StructConst, "EX_StructConst" },
		{ EX_EndStructConst, "EX_EndStructConst" },
		{ EX_SetArray, "EX_SetArray" },
		{ EX_EndArray, "EX_EndArray" },
		{ EX_UnicodeStringConst, "EX_UnicodeStringConst" },
		{ EX_Int64Const, "EX_Int64Const" },
		{ EX_UInt64Const, "EX_UInt64Const" },
		{ EX_PrimitiveCast, "EX_PrimitiveCast" },
		{ EX_SetSet, "EX_SetSet" },
		{ EX_EndSet, "EX_EndSet" },
		{ EX_SetMap, "EX_SetMap" },
		{ EX_EndMap, "EX_EndMap" },
		{ EX_SetConst, "EX_SetConst" },
		{ EX_EndSetConst, "EX_EndSetConst" },
		{ EX_MapConst, "EX_MapConst" },
		{ EX_EndMapConst, "EX_EndMapConst" },
		{ EX_StructMemberContext, "EX_StructMemberContext" },
		{ EX_LetMulticastDelegate, "EX_LetMulticastDelegate" },
		{ EX_LetDelegate, "EX_LetDelegate" },
		{ EX_LocalOutVariable, "EX_LocalOutVariable" },
		{ EX_DeprecatedOp4A, "EX_DeprecatedOp4A" },
		{ EX_InstanceDelegate, "EX_InstanceDelegate" },
		{ EX_PushExecutionFlow, "EX_PushExecutionFlow" },
		{ EX_PopExecutionFlow, "EX_PopExecutionFlow" },
		{ EX_ComputedJump, "EX_ComputedJump" },
		{ EX_PopExecutionFlowIfNot, "EX_PopExecutionFlowIfNot" },
		{ EX_Breakpoint, "EX_Breakpoint" },
		{ EX_InterfaceContext, "EX_InterfaceContext" },
		{ EX_ObjToInterfaceCast, "EX_ObjToInterfaceCast" },
		{ EX_EndOfScript, "EX_EndOfScript" },
		{ EX_CrossInterfaceCast, "EX_CrossInterfaceCast" },
		{ EX_InterfaceToObjCast, "EX_InterfaceToObjCast" },
		{ EX_WireTracepoint, "EX_WireTracepoint" },
		{ EX_SkipOffsetConst, "EX_SkipOffsetConst" },
		{ EX_AddMulticastDelegate, "EX_AddMulticastDelegate" },
		{ EX_ClearMulticastDelegate, "EX_ClearMulticastDelegate" },
		{ EX_Tracepoint, "EX_Tracepoint" },
		{ EX_LetObj, "EX_LetObj" },
		{ EX_LetWeakObjPtr, "EX_LetWeakObjPtr" },
		{ EX_BindDelegate, "EX_BindDelegate" },
		{ EX_RemoveMulticastDelegate, "EX_RemoveMulticastDelegate" },
		{ EX_CallMulticastDelegate, "EX_CallMulticastDelegate" },
		{ EX_LetValueOnPersistentFrame, "EX_LetValueOnPersistentFrame" },
		{ EX_ArrayConst, "EX_ArrayConst" },
		{ EX_EndArrayConst, "EX_EndArrayConst" },
		{ EX_SoftObjectConst, "EX_SoftObjectConst" },
		{ EX_CallMath, "EX_CallMath" },
		{ EX_SwitchValue, "EX_SwitchValue" },
		{ EX_InstrumentationEvent, "EX_InstrumentationEvent" },
		{ EX_ArrayGetByRef, "EX_ArrayGetByRef" },
		{ EX_Max, "EX_Max" }
	};

	enum EFunctionFlags : uint32_t
	{
		// Function flags.
		FUNC_None = 0x00000000,

		FUNC_Final = 0x00000001,	// Function is final (prebindable, non-overridable function).
		FUNC_RequiredAPI = 0x00000002,	// Indicates this function is DLL exported/imported.
		FUNC_BlueprintAuthorityOnly = 0x00000004,   // Function will only run if the object has network authority
		FUNC_BlueprintCosmetic = 0x00000008,   // Function is cosmetic in nature and should not be invoked on dedicated servers
		// FUNC_				= 0x00000010,   // unused.
		// FUNC_				= 0x00000020,   // unused.
		FUNC_Net = 0x00000040,   // Function is network-replicated.
		FUNC_NetReliable = 0x00000080,   // Function should be sent reliably on the network.
		FUNC_NetRequest = 0x00000100,	// Function is sent to a net service
		FUNC_Exec = 0x00000200,	// Executable from command line.
		FUNC_Native = 0x00000400,	// Native function.
		FUNC_Event = 0x00000800,   // Event function.
		FUNC_NetResponse = 0x00001000,   // Function response from a net service
		FUNC_Static = 0x00002000,   // Static function.
		FUNC_NetMulticast = 0x00004000,	// Function is networked multicast Server -> All Clients
		// FUNC_				= 0x00008000,   // unused.
		FUNC_MulticastDelegate = 0x00010000,	// Function is a multi-cast delegate signature (also requires FUNC_Delegate to be set!)
		FUNC_Public = 0x00020000,	// Function is accessible in all classes (if overridden, parameters must remain unchanged).
		FUNC_Private = 0x00040000,	// Function is accessible only in the class it is defined in (cannot be overridden, but function name may be reused in subclasses.  IOW: if overridden, parameters don't need to match, and Super.Func() cannot be accessed since it's private.)
		FUNC_Protected = 0x00080000,	// Function is accessible only in the class it is defined in and subclasses (if overridden, parameters much remain unchanged).
		FUNC_Delegate = 0x00100000,	// Function is delegate signature (either single-cast or multi-cast, depending on whether FUNC_MulticastDelegate is set.)
		FUNC_NetServer = 0x00200000,	// Function is executed on servers (set by replication code if passes check)
		FUNC_HasOutParms = 0x00400000,	// function has out (pass by reference) parameters
		FUNC_HasDefaults = 0x00800000,	// function has structs that contain defaults
		FUNC_NetClient = 0x01000000,	// function is executed on clients
		FUNC_DLLImport = 0x02000000,	// function is imported from a DLL
		FUNC_BlueprintCallable = 0x04000000,	// function can be called from blueprint code
		FUNC_BlueprintEvent = 0x08000000,	// function can be overridden/implemented from a blueprint
		FUNC_BlueprintPure = 0x10000000,	// function can be called from blueprint code, and is also pure (produces no side effects). If you set this, you should set FUNC_BlueprintCallable as well.
		FUNC_EditorOnly = 0x20000000,	// function can only be called from an editor scrippt.
		FUNC_Const = 0x40000000,	// function can be called from blueprint code, and only reads state (never writes state)
		FUNC_NetValidate = 0x80000000,	// function must supply a _Validate implementation

		FUNC_AllFlags = 0xFFFFFFFF,
	};

	struct UFunction : UObject
	{
		unsigned char pad_0[72];     // +00  00
		TArray<uint8_t> Script;      // +48  72
		unsigned char pad_1[32];     // +58  88
		TArray<UObject*> ScriptObjectReferences;// +78  120
		EFunctionFlags Flags;        // +88  136
	};

}