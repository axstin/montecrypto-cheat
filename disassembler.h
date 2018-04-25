#pragma once

/*=============================================================================
Disassembler for Kismet bytecode.

https://github.com/EpicGames/UnrealEngine/blob/4.18/Engine/Source/Editor/UnrealEd/Private/ScriptDisassembler.cpp
I adapted this from ScriptDisassembler.cpp, a file that is included only in editor builds of UE4, for Montecrypto
NOTE: It is more than likely this may work on other UE4 games as well
=============================================================================*/

#include <string>

#include "unrealapi.h"

using namespace Unreal;

namespace EScriptInstrumentation
{
	enum Type
	{
		Class = 0,
		ClassScope,
		Instance,
		Event,
		InlineEvent,
		ResumeEvent,
		PureNodeEntry,
		NodeDebugSite,
		NodeEntry,
		NodeExit,
		PushState,
		RestoreState,
		ResetState,
		SuspendState,
		PopState,
		TunnelEndOfThread,
		Stop
	};
}

/**
* Kismet bytecode disassembler; Can be used to create a human readable version
* of Kismet bytecode for a specified structure or class.
*/
class FKismetBytecodeDisassembler
{
private:
	const uint8_t* Script;
	size_t ScriptNum;
	std::string Indents;
	FILE* Ar;
public:
	/**
	* Construct a disassembler that will output to the specified archive.
	*
	* @param	InAr	The archive to emit disassembled bytecode to.
	*/
	FKismetBytecodeDisassembler(FILE* InAr)
		: Ar(InAr)
	{
		InitTables();
	}

	/**
	* Disassemble all of the script code in a single structure.
	*
	* @param [in,out]	Source	The structure to disassemble.
	*/
	void DisassembleStructure(UFunction* Source)
	{
		ScriptNum = Source->Script.Num;
		Script = new uint8_t[ScriptNum];
		memcpy((void*)Script, Source->Script.Data, ScriptNum);

		int32_t ScriptIndex = 0;
		while (ScriptIndex < ScriptNum)
		{
			fprintf(Ar, "Label_0x%X:\n", ScriptIndex);

			AddIndent();
			SerializeExpr(ScriptIndex);
			DropIndent();
		}
	}

	/**
	* Disassemble all functions in any classes that have matching names.
	*
	* @param	InAr	The archive to emit disassembled bytecode to.
	* @param	ClassnameSubstring	A class must contain this substring to be disassembled.
	*/
	static void DisassembleAllFunctionsInClasses(FILE* Ar, const std::string& ClassnameSubstring)
	{
		throw;
	}

	enum class EBlueprintTextLiteralType : uint8_t
	{
		/* Text is an empty string. The bytecode contains no strings, and you should use FText::GetEmpty() to initialize the FText instance. */
		Empty,
		/** Text is localized. The bytecode will contain three strings - source, key, and namespace - and should be loaded via FInternationalization */
		LocalizedText,
		/** Text is culture invariant. The bytecode will contain one string, and you should use FText::AsCultureInvariant to initialize the FText instance. */
		InvariantText,
		/** Text is a literal FString. The bytecode will contain one string, and you should use FText::FromString to initialize the FText instance. */
		LiteralString,
		/** Text is from a string table. The bytecode will contain an object pointer (not used) and two strings - the table ID, and key - and should be found via FText::FromStringTable */
		StringTableEntry,
	};

private:

	// Reading functions
	int32_t ReadINT(int32_t& ScriptIndex)
	{
		int32_t Value = Script[ScriptIndex]; ++ScriptIndex;
		Value = Value | ((int32_t)Script[ScriptIndex] << 8); ++ScriptIndex;
		Value = Value | ((int32_t)Script[ScriptIndex] << 16); ++ScriptIndex;
		Value = Value | ((int32_t)Script[ScriptIndex] << 24); ++ScriptIndex;

		return Value;
	}

	uint64_t ReadQWORD(int32_t& ScriptIndex)
	{
		uint64_t Value = Script[ScriptIndex]; ++ScriptIndex;
		Value = Value | ((uint64_t)Script[ScriptIndex] << 8); ++ScriptIndex;
		Value = Value | ((uint64_t)Script[ScriptIndex] << 16); ++ScriptIndex;
		Value = Value | ((uint64_t)Script[ScriptIndex] << 24); ++ScriptIndex;
		Value = Value | ((uint64_t)Script[ScriptIndex] << 32); ++ScriptIndex;
		Value = Value | ((uint64_t)Script[ScriptIndex] << 40); ++ScriptIndex;
		Value = Value | ((uint64_t)Script[ScriptIndex] << 48); ++ScriptIndex;
		Value = Value | ((uint64_t)Script[ScriptIndex] << 56); ++ScriptIndex;

		return Value;
	}

	uint8_t ReadBYTE(int32_t& ScriptIndex)
	{
		uint8_t Value = Script[ScriptIndex]; ++ScriptIndex;

		return Value;
	}

	std::string ReadName(int32_t& ScriptIndex)
	{


		const FScriptName ConstValue = *(FScriptName*)(Script + ScriptIndex);
		ScriptIndex += sizeof(FScriptName);

		return FName(ConstValue).ToString();
	}

	uint16_t ReadWORD(int32_t& ScriptIndex)
	{
		uint16_t Value = Script[ScriptIndex]; ++ScriptIndex;
		Value = Value | ((uint16_t)Script[ScriptIndex] << 8); ++ScriptIndex;
		return Value;
	}

	float ReadFLOAT(int32_t& ScriptIndex)
	{
		union { float f; int32_t i; } Result;
		Result.i = ReadINT(ScriptIndex);
		return Result.f;
	}

	uint32_t ReadSkipCount(int32_t& ScriptIndex)
	{
		//#if SCRIPT_LIMIT_BYTECODE_TO_64KB
		//	return ReadWORD(ScriptIndex);
		//#else
		//	static_assert(sizeof(uint32_t) == 4, "Update this code as size changed.");
		return ReadINT(ScriptIndex);
		//#endif
	}

	std::wstring ReadString(int32_t& ScriptIndex)
	{
		const EExprToken Opcode = (EExprToken)Script[ScriptIndex++];

		switch (Opcode)
		{
		case EX_StringConst:
			return ReadString8(ScriptIndex);

		case EX_UnicodeStringConst:
			return ReadString16(ScriptIndex);

		default:
			//checkf(false, TEXT("FKismetBytecodeDisassembler::ReadString - Unexpected opcode. Expected %d or %d, got %d"), EX_StringConst, EX_UnicodeStringConst, Opcode);
			return L"ERROR: FKismetBytecodeDisassembler::ReadString - Unexpected opcode";
		}
	}

	std::wstring ReadString8(int32_t& ScriptIndex)
	{
		std::wstring Result;

		do
		{
			Result += (char)ReadBYTE(ScriptIndex);
		} while (Script[ScriptIndex - 1] != 0);

		return Result;
	}

	std::wstring ReadString16(int32_t& ScriptIndex)
	{
		std::wstring Result;

		do
		{
			Result += (wchar_t)ReadWORD(ScriptIndex);
		} while ((Script[ScriptIndex - 1] != 0) || (Script[ScriptIndex - 2] != 0));

		return Result;
	}

	EExprToken SerializeExpr(int32_t& ScriptIndex)
	{
		AddIndent();

		EExprToken Opcode = (EExprToken)Script[ScriptIndex];
		ScriptIndex++;

		ProcessCommon(ScriptIndex, Opcode);

		DropIndent();

		return Opcode;
	}

	void ProcessCastByte(int32_t CastType, int32_t& ScriptIndex)
	{
		// Expression of cast
		SerializeExpr(ScriptIndex);
	}

	void ProcessCommon(int32_t& ScriptIndex, EExprToken Opcode)
	{
		switch (Opcode)
		{
		case EX_PrimitiveCast:
		{
			// A type conversion.
			uint8_t ConversionType = ReadBYTE(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: PrimitiveCast of type %d\n"), Indents.c_str(), (int32_t)Opcode, ConversionType);
			AddIndent();

			fprintf(Ar, TEXT("%s Argument:\n"), Indents.c_str());
			ProcessCastByte(ConversionType, ScriptIndex);

			//@TODO:
			//fprintf(Ar, TEXT("%s Expression:\n"), Indents.c_str());
			//SerializeExpr( ScriptIndex );
			break;
		}
		case EX_SetSet:
		{
			fprintf(Ar, TEXT("%s $%X: set set\n"), Indents.c_str(), (int32_t)Opcode);
			SerializeExpr(ScriptIndex);
			ReadINT(ScriptIndex);
			while (SerializeExpr(ScriptIndex) != EX_EndSet)
			{
				// Set contents
			}
			break;
		}
		case EX_EndSet:
		{
			fprintf(Ar, TEXT("%s $%X: EX_EndSet\n"), Indents.c_str(), (int32_t)Opcode);
			break;
		}
		case EX_SetConst:
		{
			void* InnerProp = ReadPointer<void*>(ScriptIndex);
			int32_t Num = ReadINT(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: set set const - elements number: %d, inner property: %s\n"), Indents.c_str(), (int32_t)Opcode, Num, FName::GetObjectName(InnerProp).c_str());
			while (SerializeExpr(ScriptIndex) != EX_EndSetConst)
			{
				// Set contents
			}
			break;
		}
		case EX_EndSetConst:
		{
			fprintf(Ar, TEXT("%s $%X: EX_EndSetConst\n"), Indents.c_str(), (int32_t)Opcode);
			break;
		}
		case EX_SetMap:
		{
			fprintf(Ar, TEXT("%s $%X: set map\n"), Indents.c_str(), (int32_t)Opcode);
			SerializeExpr(ScriptIndex);
			ReadINT(ScriptIndex);
			while (SerializeExpr(ScriptIndex) != EX_EndMap)
			{
				// Map contents
			}
			break;
		}
		case EX_EndMap:
		{
			fprintf(Ar, TEXT("%s $%X: EX_EndMap\n"), Indents.c_str(), (int32_t)Opcode);
			break;
		}
		case EX_MapConst:
		{
			void* KeyProp = ReadPointer<void*>(ScriptIndex);
			void* ValProp = ReadPointer<void*>(ScriptIndex);
			int32_t Num = ReadINT(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: set map const - elements number: %d, key property: %s, val property: %s\n"), Indents.c_str(), (int32_t)Opcode, Num, FName::GetObjectName(KeyProp).c_str(), FName::GetObjectName(ValProp).c_str());
			while (SerializeExpr(ScriptIndex) != EX_EndMapConst)
			{
				// Map contents
			}
			break;
		}
		case EX_EndMapConst:
		{
			fprintf(Ar, TEXT("%s $%X: EX_EndMapConst\n"), Indents.c_str(), (int32_t)Opcode);
			break;
		}
		case EX_ObjToInterfaceCast:
		{
			// A conversion from an object variable to a native interface variable.
			// We use a different bytecode to avoid the branching each time we process a cast token

			// the interface class to convert to
			void* InterfaceClass = ReadPointer<void*>(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: ObjToInterfaceCast to %s\n"), Indents.c_str(), (int32_t)Opcode, FName::GetObjectName(InterfaceClass).c_str());

			SerializeExpr(ScriptIndex);
			break;
		}
		case EX_CrossInterfaceCast:
		{
			// A conversion from one interface variable to a different interface variable.
			// We use a different bytecode to avoid the branching each time we process a cast token

			// the interface class to convert to
			void* InterfaceClass = ReadPointer<void*>(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: InterfaceToInterfaceCast to %s\n"), Indents.c_str(), (int32_t)Opcode, FName::GetObjectName(InterfaceClass).c_str());

			SerializeExpr(ScriptIndex);
			break;
		}
		case EX_InterfaceToObjCast:
		{
			// A conversion from an interface variable to a object variable.
			// We use a different bytecode to avoid the branching each time we process a cast token

			// the interface class to convert to
			void* ObjectClass = ReadPointer<void*>(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: InterfaceToObjCast to %s\n"), Indents.c_str(), (int32_t)Opcode, FName::GetObjectName(ObjectClass).c_str());

			SerializeExpr(ScriptIndex);
			break;
		}
		case EX_Let:
		{
			fprintf(Ar, TEXT("%s $%X: Let (Variable = Expression)\n"), Indents.c_str(), (int32_t)Opcode);
			AddIndent();

			ReadPointer<void*>(ScriptIndex);

			// Variable expr.
			fprintf(Ar, TEXT("%s Variable:\n"), Indents.c_str());
			SerializeExpr(ScriptIndex);

			// Assignment expr.
			fprintf(Ar, TEXT("%s Expression:\n"), Indents.c_str());
			SerializeExpr(ScriptIndex);

			DropIndent();
			break;
		}
		case EX_LetObj:
		case EX_LetWeakObjPtr:
		{
			if (Opcode == EX_LetObj)
			{
				fprintf(Ar, TEXT("%s $%X: Let Obj (Variable = Expression)\n"), Indents.c_str(), (int32_t)Opcode);
			}
			else
			{
				fprintf(Ar, TEXT("%s $%X: Let WeakObjPtr (Variable = Expression)\n"), Indents.c_str(), (int32_t)Opcode);
			}
			AddIndent();

			// Variable expr.
			fprintf(Ar, TEXT("%s Variable:\n"), Indents.c_str());
			SerializeExpr(ScriptIndex);

			// Assignment expr.
			fprintf(Ar, TEXT("%s Expression:\n"), Indents.c_str());
			SerializeExpr(ScriptIndex);

			DropIndent();
			break;
		}
		case EX_LetBool:
		{
			fprintf(Ar, TEXT("%s $%X: LetBool (Variable = Expression)\n"), Indents.c_str(), (int32_t)Opcode);
			AddIndent();

			// Variable expr.
			fprintf(Ar, TEXT("%s Variable:\n"), Indents.c_str());
			SerializeExpr(ScriptIndex);

			// Assignment expr.
			fprintf(Ar, TEXT("%s Expression:\n"), Indents.c_str());
			SerializeExpr(ScriptIndex);

			DropIndent();
			break;
		}
		case EX_LetValueOnPersistentFrame:
		{
			fprintf(Ar, TEXT("%s $%X: LetValueOnPersistentFrame\n"), Indents.c_str(), (int32_t)Opcode);
			AddIndent();

			auto Prop = ReadPointer<void*>(ScriptIndex);
			fprintf(Ar, TEXT("%s Destination variable: %s, offset: N/A\n"), Indents.c_str(), FName::GetObjectName(Prop).c_str()/*,
																															   Prop ? Prop->GetOffset_ForDebug() : 0*/);

			fprintf(Ar, TEXT("%s Expression:\n"), Indents.c_str());
			SerializeExpr(ScriptIndex);

			DropIndent();

			break;
		}
		case EX_StructMemberContext:
		{
			fprintf(Ar, TEXT("%s $%X: Struct member context \n"), Indents.c_str(), (int32_t)Opcode);
			AddIndent();

			void* Prop = ReadPointer<void*>(ScriptIndex);

			fprintf(Ar, TEXT("%s Expression within struct %s, offset N/A\n"), Indents.c_str(), FName::GetObjectName(Prop).c_str()/*,
																																 Prop->GetOffset_ForDebug()*/); // although that isn't a UFunction, we are not going to indirect the props of a struct, so this should be fine

			fprintf(Ar, TEXT("%s Expression to struct:\n"), Indents.c_str());
			SerializeExpr(ScriptIndex);

			DropIndent();

			break;
		}
		case EX_LetDelegate:
		{
			fprintf(Ar, TEXT("%s $%X: LetDelegate (Variable = Expression)\n"), Indents.c_str(), (int32_t)Opcode);
			AddIndent();

			// Variable expr.
			fprintf(Ar, TEXT("%s Variable:\n"), Indents.c_str());
			SerializeExpr(ScriptIndex);

			// Assignment expr.
			fprintf(Ar, TEXT("%s Expression:\n"), Indents.c_str());
			SerializeExpr(ScriptIndex);

			DropIndent();
			break;
		}
		case EX_LetMulticastDelegate:
		{
			fprintf(Ar, TEXT("%s $%X: LetMulticastDelegate (Variable = Expression)\n"), Indents.c_str(), (int32_t)Opcode);
			AddIndent();

			// Variable expr.
			fprintf(Ar, TEXT("%s Variable:\n"), Indents.c_str());
			SerializeExpr(ScriptIndex);

			// Assignment expr.
			fprintf(Ar, TEXT("%s Expression:\n"), Indents.c_str());
			SerializeExpr(ScriptIndex);

			DropIndent();
			break;
		}

		case EX_ComputedJump:
		{
			fprintf(Ar, TEXT("%s $%X: Computed Jump, offset specified by expression:\n"), Indents.c_str(), (int32_t)Opcode);

			AddIndent();
			SerializeExpr(ScriptIndex);
			DropIndent();

			break;
		}

		case EX_Jump:
		{
			uint32_t SkipCount = ReadSkipCount(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: Jump to offset 0x%X\n"), Indents.c_str(), (int32_t)Opcode, SkipCount);
			break;
		}
		case EX_LocalVariable:
		{
			void* PropertyPtr = ReadPointer<void*>(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: Local variable named %s (%p)\n"), Indents.c_str(), (int32_t)Opcode, PropertyPtr ? FName::GetObjectName(PropertyPtr).c_str() : TEXT("(null)\n"), PropertyPtr);
			break;
		}
		case EX_DefaultVariable:
		{
			void* PropertyPtr = ReadPointer<void*>(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: Default variable named %s (%p)\n"), Indents.c_str(), (int32_t)Opcode, PropertyPtr ? FName::GetObjectName(PropertyPtr).c_str() : TEXT("(null)\n"), PropertyPtr);
			break;
		}
		case EX_InstanceVariable:
		{
			void* PropertyPtr = ReadPointer<void*>(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: Instance variable named %s (%p)\n"), Indents.c_str(), (int32_t)Opcode, PropertyPtr ? FName::GetObjectName(PropertyPtr).c_str() : TEXT("(null)\n"), PropertyPtr);
			break;
		}
		case EX_LocalOutVariable:
		{
			void* PropertyPtr = ReadPointer<void*>(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: Local out variable named %s (%p)\n"), Indents.c_str(), (int32_t)Opcode, PropertyPtr ? FName::GetObjectName(PropertyPtr).c_str() : TEXT("(null)\n"), PropertyPtr);
			break;
		}
		case EX_InterfaceContext:
		{
			fprintf(Ar, TEXT("%s $%X: EX_InterfaceContext:\n"), Indents.c_str(), (int32_t)Opcode);
			SerializeExpr(ScriptIndex);
			break;
		}
		case EX_DeprecatedOp4A:
		{
			fprintf(Ar, TEXT("%s $%X: This opcode has been removed and does nothing.\n"), Indents.c_str(), (int32_t)Opcode);
			break;
		}
		case EX_Nothing:
		{
			fprintf(Ar, TEXT("%s $%X: EX_Nothing\n"), Indents.c_str(), (int32_t)Opcode);
			break;
		}
		case EX_EndOfScript:
		{
			fprintf(Ar, TEXT("%s $%X: EX_EndOfScript\n"), Indents.c_str(), (int32_t)Opcode);
			break;
		}
		case EX_EndFunctionParms:
		{
			fprintf(Ar, TEXT("%s $%X: EX_EndFunctionParms\n"), Indents.c_str(), (int32_t)Opcode);
			break;
		}
		case EX_EndStructConst:
		{
			fprintf(Ar, TEXT("%s $%X: EX_EndStructConst\n"), Indents.c_str(), (int32_t)Opcode);
			break;
		}
		case EX_EndArray:
		{
			fprintf(Ar, TEXT("%s $%X: EX_EndArray\n"), Indents.c_str(), (int32_t)Opcode);
			break;
		}
		case EX_EndArrayConst:
		{
			fprintf(Ar, TEXT("%s $%X: EX_EndArrayConst\n"), Indents.c_str(), (int32_t)Opcode);
			break;
		}
		case EX_IntZero:
		{
			fprintf(Ar, TEXT("%s $%X: EX_IntZero\n"), Indents.c_str(), (int32_t)Opcode);
			break;
		}
		case EX_IntOne:
		{
			fprintf(Ar, TEXT("%s $%X: EX_IntOne\n"), Indents.c_str(), (int32_t)Opcode);
			break;
		}
		case EX_True:
		{
			fprintf(Ar, TEXT("%s $%X: EX_True\n"), Indents.c_str(), (int32_t)Opcode);
			break;
		}
		case EX_False:
		{
			fprintf(Ar, TEXT("%s $%X: EX_False\n"), Indents.c_str(), (int32_t)Opcode);
			break;
		}
		case EX_NoObject:
		{
			fprintf(Ar, TEXT("%s $%X: EX_NoObject\n"), Indents.c_str(), (int32_t)Opcode);
			break;
		}
		case EX_NoInterface:
		{
			fprintf(Ar, TEXT("%s $%X: EX_NoObject\n"), Indents.c_str(), (int32_t)Opcode);
			break;
		}
		case EX_Self:
		{
			fprintf(Ar, TEXT("%s $%X: EX_Self\n"), Indents.c_str(), (int32_t)Opcode);
			break;
		}
		case EX_EndParmValue:
		{
			fprintf(Ar, TEXT("%s $%X: EX_EndParmValue\n"), Indents.c_str(), (int32_t)Opcode);
			break;
		}
		case EX_Return:
		{
			fprintf(Ar, TEXT("%s $%X: Return expression\n"), Indents.c_str(), (int32_t)Opcode);

			SerializeExpr(ScriptIndex); // Return expression.
			break;
		}
		case EX_CallMath:
		{
			UObject* StackNode = ReadPointer<UObject>(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: Call Math (stack node %s::%s)\n"), Indents.c_str(), (int32_t)Opcode, StackNode ? GetNameSafe(StackNode->GetParent()).c_str() : nullptr, FName::GetObjectName(StackNode).c_str());

			while (SerializeExpr(ScriptIndex) != EX_EndFunctionParms)
			{
				// Params
			}
			break;
		}
		case EX_FinalFunction:
		{
			UObject* StackNode = ReadPointer<UObject>(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: Final Function (stack node %s::%s)\n"), Indents.c_str(), (int32_t)Opcode, StackNode ? StackNode->GetParent()->GetName().c_str() : TEXT("(null)"), StackNode ? FName::GetObjectName(StackNode).c_str() : TEXT("(null)\n"));

			while (SerializeExpr(ScriptIndex) != EX_EndFunctionParms)
			{
				// Params
			}
			break;
		}
		case EX_CallMulticastDelegate:
		{
			UObject* StackNode = ReadPointer<UObject>(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: CallMulticastDelegate (signature %s::%s) delegate:"), Indents.c_str(), (int32_t)Opcode, StackNode ? StackNode->GetParent()->GetName().c_str() : TEXT("(null)"), StackNode ? FName::GetObjectName(StackNode).c_str() : TEXT("(null)\n"));
			SerializeExpr(ScriptIndex);
			fprintf(Ar, TEXT("Params:\n"));
			while (SerializeExpr(ScriptIndex) != EX_EndFunctionParms)
			{
				// Params
			}
			break;
		}
		case EX_VirtualFunction:
		{
			std::string FunctionName = ReadName(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: Virtual Function named %s\n"), Indents.c_str(), (int32_t)Opcode, FunctionName.c_str());

			while (SerializeExpr(ScriptIndex) != EX_EndFunctionParms)
			{
			}
			break;
		}
		case EX_ClassContext:
		case EX_Context:
		case EX_Context_FailSilent:
		{
			fprintf(Ar, TEXT("%s $%X: %s"), Indents.c_str(), (int32_t)Opcode, Opcode == EX_ClassContext ? TEXT("Class Context\n") : TEXT("Context\n"));
			AddIndent();

			// Object expression.
			fprintf(Ar, TEXT("%s ObjectExpression:\n"), Indents.c_str());
			SerializeExpr(ScriptIndex);

			if (Opcode == EX_Context_FailSilent)
			{
				fprintf(Ar, TEXT(" Can fail silently on access none \n"));
			}

			// Code offset for NULL expressions.
			uint32_t SkipCount = ReadSkipCount(ScriptIndex);
			fprintf(Ar, TEXT("%s Skip Bytes: 0x%X\n"), Indents.c_str(), SkipCount);
			
			// Property corresponding to the r-value data, in case the l-value needs to be mem-zero'd
			void* Field = ReadPointer<void*>(ScriptIndex);
			fprintf(Ar, TEXT("%s R-Value Property: %s\n"), Indents.c_str(), Field ? FName::GetObjectName(Field).c_str() : TEXT("(null)\n"));

			// Context expression.
			fprintf(Ar, TEXT("%s ContextExpression:\n"), Indents.c_str());
			SerializeExpr(ScriptIndex);

			DropIndent();
			break;
		}
		case EX_IntConst:
		{
			int32_t ConstValue = ReadINT(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: literal int32_t %d\n"), Indents.c_str(), (int32_t)Opcode, ConstValue);
			break;
		}
		case EX_SkipOffsetConst:
		{
			uint32_t ConstValue = ReadSkipCount(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: literal uint32_t 0x%X\n"), Indents.c_str(), (int32_t)Opcode, ConstValue);
			break;
		}
		case EX_FloatConst:
		{
			float ConstValue = ReadFLOAT(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: literal float %f\n"), Indents.c_str(), (int32_t)Opcode, ConstValue);
			break;
		}
		case EX_StringConst:
		{
			std::wstring ConstValue = ReadString8(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: literal ansi string \"%s\"\n"), Indents.c_str(), (int32_t)Opcode, WStringToString(ConstValue).c_str());
			break;
		}
		case EX_UnicodeStringConst:
		{
			std::wstring ConstValue = ReadString16(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: literal unicode string \"%s\"\n"), Indents.c_str(), (int32_t)Opcode, WStringToString(ConstValue).c_str());
			break;
		}
		case EX_TextConst:
		{
			// What kind of text are we dealing with?
			const EBlueprintTextLiteralType TextLiteralType = (EBlueprintTextLiteralType)Script[ScriptIndex++];

			switch (TextLiteralType)
			{
			case EBlueprintTextLiteralType::Empty:
			{
				fprintf(Ar, TEXT("%s $%X: literal text - empty\n"), Indents.c_str(), (int32_t)Opcode);
			}
			break;

			case EBlueprintTextLiteralType::LocalizedText:
			{
				const std::wstring SourceString = ReadString(ScriptIndex);
				const std::wstring KeyString = ReadString(ScriptIndex);
				const std::wstring Namespace = ReadString(ScriptIndex);
				fprintf(Ar, TEXT("%s $%X: literal text - localized text { namespace: \"%s\", key: \"%s\", source: \"%s\" }\n"), Indents.c_str(), (int32_t)Opcode,
					WStringToString(Namespace).c_str(), WStringToString(KeyString).c_str(), WStringToString(SourceString).c_str());
			}
			break;

			case EBlueprintTextLiteralType::InvariantText:
			{
				const std::wstring SourceString = ReadString(ScriptIndex);
				fprintf(Ar, TEXT("%s $%X: literal text - invariant text: \"%s\"\n"), Indents.c_str(), (int32_t)Opcode, WStringToString(SourceString).c_str());
			}
			break;

			case EBlueprintTextLiteralType::LiteralString:
			{
				const std::wstring SourceString = ReadString(ScriptIndex);
				fprintf(Ar, TEXT("%s $%X: literal text - literal string: \"%s\"\n"), Indents.c_str(), (int32_t)Opcode, WStringToString(SourceString).c_str());
			}
			break;

			case EBlueprintTextLiteralType::StringTableEntry:
			{
				ReadPointer<void*>(ScriptIndex); // String Table asset (if any)
				const std::wstring TableIdString = ReadString(ScriptIndex);
				const std::wstring KeyString = ReadString(ScriptIndex);
				fprintf(Ar, TEXT("%s $%X: literal text - string table entry { tableid: \"%s\", key: \"%s\" }\n"), Indents.c_str(), (int32_t)Opcode, WStringToString(TableIdString).c_str(), WStringToString(KeyString).c_str());
			}
			break;

			default:
				fprintf(Ar, TEXT("Unknown EBlueprintTextLiteralType! Please update FKismetBytecodeDisassembler::ProcessCommon to handle this type of text.\n"));
				break;
			}
			break;
		}
		case EX_ObjectConst:
		{
			void* Pointer = ReadPointer<void*>(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: EX_ObjectConst (%p:%s)\n"), Indents.c_str(), (int32_t)Opcode, Pointer, FName::GetObjectName(Pointer).c_str());
			break;
		}
		case EX_SoftObjectConst:
		{
			fprintf(Ar, TEXT("%s $%X: EX_SoftObjectConst\n"), Indents.c_str(), (int32_t)Opcode);
			SerializeExpr(ScriptIndex);
			break;
		}
		case EX_NameConst:
		{
			std::string ConstValue = ReadName(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: literal name %s\n"), Indents.c_str(), (int32_t)Opcode, ConstValue.c_str());
			break;
		}
		case EX_RotationConst:
		{
			float Pitch = ReadFLOAT(ScriptIndex);
			float Yaw = ReadFLOAT(ScriptIndex);
			float Roll = ReadFLOAT(ScriptIndex);

			fprintf(Ar, TEXT("%s $%X: literal rotation (%f,%f,%f)\n"), Indents.c_str(), (int32_t)Opcode, Pitch, Yaw, Roll);
			break;
		}
		case EX_VectorConst:
		{
			float X = ReadFLOAT(ScriptIndex);
			float Y = ReadFLOAT(ScriptIndex);
			float Z = ReadFLOAT(ScriptIndex);

			fprintf(Ar, TEXT("%s $%X: literal vector (%f,%f,%f)\n"), Indents.c_str(), (int32_t)Opcode, X, Y, Z);
			break;
		}
		case EX_TransformConst:
		{

			float RotX = ReadFLOAT(ScriptIndex);
			float RotY = ReadFLOAT(ScriptIndex);
			float RotZ = ReadFLOAT(ScriptIndex);
			float RotW = ReadFLOAT(ScriptIndex);

			float TransX = ReadFLOAT(ScriptIndex);
			float TransY = ReadFLOAT(ScriptIndex);
			float TransZ = ReadFLOAT(ScriptIndex);

			float ScaleX = ReadFLOAT(ScriptIndex);
			float ScaleY = ReadFLOAT(ScriptIndex);
			float ScaleZ = ReadFLOAT(ScriptIndex);

			fprintf(Ar, TEXT("%s $%X: literal transform R(%f,%f,%f,%f) T(%f,%f,%f) S(%f,%f,%f)\n"), Indents.c_str(), (int32_t)Opcode, TransX, TransY, TransZ, RotX, RotY, RotZ, RotW, ScaleX, ScaleY, ScaleZ);
			break;
		}
		case EX_StructConst:
		{
			void* Struct = ReadPointer<void*>(ScriptIndex);
			int32_t SerializedSize = ReadINT(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: literal struct %s (serialized size: %d)\n"), Indents.c_str(), (int32_t)Opcode, FName::GetObjectName(Struct).c_str(), SerializedSize);
			while (SerializeExpr(ScriptIndex) != EX_EndStructConst)
			{
				// struct contents
			}
			break;
		}
		case EX_SetArray:
		{
			fprintf(Ar, TEXT("%s $%X: set array\n"), Indents.c_str(), (int32_t)Opcode);
			SerializeExpr(ScriptIndex);
			while (SerializeExpr(ScriptIndex) != EX_EndArray)
			{
				// Array contents
			}
			break;
		}
		case EX_ArrayConst:
		{
			void* InnerProp = ReadPointer<void*>(ScriptIndex);
			int32_t Num = ReadINT(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: set array const - elements number: %d, inner property: %s\n"), Indents.c_str(), (int32_t)Opcode, Num, FName::GetObjectName(InnerProp).c_str());
			while (SerializeExpr(ScriptIndex) != EX_EndArrayConst)
			{
				// Array contents
			}
			break;
		}
		case EX_ByteConst:
		{
			uint8_t ConstValue = ReadBYTE(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: literal byte %d\n"), Indents.c_str(), (int32_t)Opcode, ConstValue);
			break;
		}
		case EX_IntConstByte:
		{
			int32_t ConstValue = ReadBYTE(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: literal int %d\n"), Indents.c_str(), (int32_t)Opcode, ConstValue);
			break;
		}
		case EX_MetaCast:
		{
			void* Class = ReadPointer<void*>(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: MetaCast to %s of expr:\n"), Indents.c_str(), (int32_t)Opcode, FName::GetObjectName(Class).c_str());
			SerializeExpr(ScriptIndex);
			break;
		}
		case EX_DynamicCast:
		{
			void* Class = ReadPointer<void*>(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: DynamicCast to %s of expr:\n"), Indents.c_str(), (int32_t)Opcode, FName::GetObjectName(Class).c_str());
			SerializeExpr(ScriptIndex);
			break;
		}
		case EX_JumpIfNot:
		{
			// Code offset.
			uint32_t SkipCount = ReadSkipCount(ScriptIndex);

			fprintf(Ar, TEXT("%s $%X: Jump to offset 0x%X if not expr:\n"), Indents.c_str(), (int32_t)Opcode, SkipCount);

			// Boolean expr.
			SerializeExpr(ScriptIndex);
			break;
		}
		case EX_Assert:
		{
			uint16_t LineNumber = ReadWORD(ScriptIndex);
			uint8_t InDebugMode = ReadBYTE(ScriptIndex);

			fprintf(Ar, TEXT("%s $%X: assert at line %d, in debug mode = %d with expr:\n"), Indents.c_str(), (int32_t)Opcode, LineNumber, InDebugMode);
			SerializeExpr(ScriptIndex); // Assert expr.
			break;
		}
		case EX_Skip:
		{
			uint32_t W = ReadSkipCount(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: possibly skip 0x%X bytes of expr:\n"), Indents.c_str(), (int32_t)Opcode, W);

			// Expression to possibly skip.
			SerializeExpr(ScriptIndex);

			break;
		}
		case EX_InstanceDelegate:
		{
			// the name of the function assigned to the delegate.
			std::string FuncName = ReadName(ScriptIndex);

			fprintf(Ar, TEXT("%s $%X: instance delegate function named %s\n"), Indents.c_str(), (int32_t)Opcode, FuncName.c_str());
			break;
		}
		case EX_AddMulticastDelegate:
		{
			fprintf(Ar, TEXT("%s $%X: Add MC delegate\n"), Indents.c_str(), (int32_t)Opcode);
			SerializeExpr(ScriptIndex);
			SerializeExpr(ScriptIndex);
			break;
		}
		case EX_RemoveMulticastDelegate:
		{
			fprintf(Ar, TEXT("%s $%X: Remove MC delegate\n"), Indents.c_str(), (int32_t)Opcode);
			SerializeExpr(ScriptIndex);
			SerializeExpr(ScriptIndex);
			break;
		}
		case EX_ClearMulticastDelegate:
		{
			fprintf(Ar, TEXT("%s $%X: Clear MC delegate\n"), Indents.c_str(), (int32_t)Opcode);
			SerializeExpr(ScriptIndex);
			break;
		}
		case EX_BindDelegate:
		{
			// the name of the function assigned to the delegate.
			std::string FuncName = ReadName(ScriptIndex);

			fprintf(Ar, TEXT("%s $%X: BindDelegate '%s' \n"), Indents.c_str(), (int32_t)Opcode, FuncName.c_str());

			fprintf(Ar, TEXT("%s Delegate:\n"), Indents.c_str());
			SerializeExpr(ScriptIndex);

			fprintf(Ar, TEXT("%s Object:\n"), Indents.c_str());
			SerializeExpr(ScriptIndex);

			break;
		}
		case EX_PushExecutionFlow:
		{
			uint32_t SkipCount = ReadSkipCount(ScriptIndex);
			fprintf(Ar, TEXT("%s $%X: FlowStack.Push(0x%X);\n"), Indents.c_str(), (int32_t)Opcode, SkipCount);
			break;
		}
		case EX_PopExecutionFlow:
		{
			fprintf(Ar, TEXT("%s $%X: if (FlowStack.Num()) { jump to statement at FlowStack.Pop(); } else { ERROR!!! }\n"), Indents.c_str(), (int32_t)Opcode);
			break;
		}
		case EX_PopExecutionFlowIfNot:
		{
			fprintf(Ar, TEXT("%s $%X: if (!condition) { if (FlowStack.Num()) { jump to statement at FlowStack.Pop(); } else { ERROR!!! } }\n"), Indents.c_str(), (int32_t)Opcode);
			// Boolean expr.
			SerializeExpr(ScriptIndex);
			break;
		}
		case EX_Breakpoint:
		{
			fprintf(Ar, TEXT("%s $%X: <<< BREAKPOINT >>>\n"), Indents.c_str(), (int32_t)Opcode);
			break;
		}
		case EX_WireTracepoint:
		{
			fprintf(Ar, TEXT("%s $%X: .. wire debug site ..\n"), Indents.c_str(), (int32_t)Opcode);
			break;
		}
		case EX_InstrumentationEvent:
		{
			const uint8_t EventType = ReadBYTE(ScriptIndex);
			switch (EventType)
			{
			case EScriptInstrumentation::InlineEvent:
				fprintf(Ar, TEXT("%s $%X: .. instrumented inline event ..\n"), Indents.c_str(), (int32_t)Opcode);
				break;
			case EScriptInstrumentation::Stop:
				fprintf(Ar, TEXT("%s $%X: .. instrumented event stop ..\n"), Indents.c_str(), (int32_t)Opcode);
				break;
			case EScriptInstrumentation::PureNodeEntry:
				fprintf(Ar, TEXT("%s $%X: .. instrumented pure node entry site ..\n"), Indents.c_str(), (int32_t)Opcode);
				break;
			case EScriptInstrumentation::NodeDebugSite:
				fprintf(Ar, TEXT("%s $%X: .. instrumented debug site ..\n"), Indents.c_str(), (int32_t)Opcode);
				break;
			case EScriptInstrumentation::NodeEntry:
				fprintf(Ar, TEXT("%s $%X: .. instrumented wire entry site ..\n"), Indents.c_str(), (int32_t)Opcode);
				break;
			case EScriptInstrumentation::NodeExit:
				fprintf(Ar, TEXT("%s $%X: .. instrumented wire exit site ..\n"), Indents.c_str(), (int32_t)Opcode);
				break;
			case EScriptInstrumentation::PushState:
				fprintf(Ar, TEXT("%s $%X: .. push execution state ..\n"), Indents.c_str(), (int32_t)Opcode);
				break;
			case EScriptInstrumentation::RestoreState:
				fprintf(Ar, TEXT("%s $%X: .. restore execution state ..\n"), Indents.c_str(), (int32_t)Opcode);
				break;
			case EScriptInstrumentation::ResetState:
				fprintf(Ar, TEXT("%s $%X: .. reset execution state ..\n"), Indents.c_str(), (int32_t)Opcode);
				break;
			case EScriptInstrumentation::SuspendState:
				fprintf(Ar, TEXT("%s $%X: .. suspend execution state ..\n"), Indents.c_str(), (int32_t)Opcode);
				break;
			case EScriptInstrumentation::PopState:
				fprintf(Ar, TEXT("%s $%X: .. pop execution state ..\n"), Indents.c_str(), (int32_t)Opcode);
				break;
			case EScriptInstrumentation::TunnelEndOfThread:
				fprintf(Ar, TEXT("%s $%X: .. tunnel end of thread ..\n"), Indents.c_str(), (int32_t)Opcode);
				break;
			}
			break;
		}
		case EX_Tracepoint:
		{
			fprintf(Ar, TEXT("%s $%X: .. debug site ..\n"), Indents.c_str(), (int32_t)Opcode);
			break;
		}
		case EX_SwitchValue:
		{
			const auto NumCases = ReadWORD(ScriptIndex);
			const auto AfterSkip = ReadSkipCount(ScriptIndex);

			fprintf(Ar, TEXT("%s $%X: Switch Value %d cases, end in 0x%X\n"), Indents.c_str(), (int32_t)Opcode, NumCases, AfterSkip);
			AddIndent();
			fprintf(Ar, TEXT("%s Index:\n"), Indents.c_str());
			SerializeExpr(ScriptIndex);

			for (uint16_t CaseIndex = 0; CaseIndex < NumCases; ++CaseIndex)
			{
				fprintf(Ar, TEXT("%s [%d] Case Index (label: 0x%X):\n"), Indents.c_str(), CaseIndex, ScriptIndex);
				SerializeExpr(ScriptIndex);	// case index value term
				const auto OffsetToNextCase = ReadSkipCount(ScriptIndex);
				fprintf(Ar, TEXT("%s [%d] Offset to the next case: 0x%X\n"), Indents.c_str(), CaseIndex, OffsetToNextCase);
				fprintf(Ar, TEXT("%s [%d] Case Result:\n"), Indents.c_str(), CaseIndex);
				SerializeExpr(ScriptIndex);	// case term
			}

			fprintf(Ar, TEXT("%s Default result (label: 0x%X):\n"), Indents.c_str(), ScriptIndex);
			SerializeExpr(ScriptIndex);
			fprintf(Ar, TEXT("%s (label: 0x%X)\n"), Indents.c_str(), ScriptIndex);
			DropIndent();
			break;
		}
		case EX_ArrayGetByRef:
		{
			fprintf(Ar, TEXT("%s $%X: Array Get-by-Ref Index\n"), Indents.c_str(), (int32_t)Opcode);
			AddIndent();
			SerializeExpr(ScriptIndex);
			SerializeExpr(ScriptIndex);
			DropIndent();
			break;
		}
		default:
		{
			// This should never occur.
			fprintf(Ar, "ERROR: Unknown bytecode 0x%02X; ignoring it\n", (uint8_t)Opcode);
			break;
		}
		}
	}

	void InitTables()
	{
	}

	template<typename T>
	void Skip(int32_t& ScriptIndex)
	{
		ScriptIndex += sizeof(T);
	}

	void AddIndent()
	{
		Indents += TEXT("  ");
	}

	void DropIndent()
	{
		// Blah, this is awful
		// Indents = Indents.Left(Indents.Len() - 2);
		Indents = Indents.substr(0, Indents.size() - 2);
	}

	template <typename T>
	T* ReadPointer(int32_t& ScriptIndex)
	{
		return (T*)ReadQWORD(ScriptIndex);
	}
};
