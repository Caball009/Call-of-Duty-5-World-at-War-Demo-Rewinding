#ifndef __UTILS_H
#define __UTILS_H

#include <Windows.h>
#include <psapi.h>

#include <memory>
#include <string>
#include <vector>

typedef unsigned char byte;
typedef int qboolean;

enum filestreamState 
{
	notInitialized,
	failedInitialization,
	fullyInitialized
};

enum rewindState
{
	notRewinding,
	rewindOne,
	rewindAll
};

enum restoreState
{
	storeAddress,
	restoreAllAddresses,
	restoreOneAddress
};

class Utils
{
public:
	static bool Re_StoreBytesWrapper(uint32_t address, uint32_t size, uint32_t mode);
	static void* TrampolineHook(byte* src, byte* dst, int len, bool stolenBytes);
	static bool WriteBytes(uint32_t address, std::string bytes, bool storeBytes);
	//static DWORD SignatureScanner(const std::string& module, std::string signature);

private:
	static bool Hook(byte* src, byte* dst, int size);
	//static void ReplaceSubstring(std::string& str, const std::string& substr1, const std::string& substr2);
	//static DWORD FindAddress(const MODULEINFO& Process, const std::unique_ptr<byte[]>& bytes, const std::vector<uint32_t>& maskPositions, uint32_t size);
	static bool RestoreBytes(const std::tuple<uint32_t, std::unique_ptr<byte[]>, uint32_t>& tuple);
	static void StoreBytes(std::vector<std::tuple<uint32_t, std::unique_ptr<byte[]>, uint32_t>>& orgBytes, uint32_t address, uint32_t size);
};

typedef union qfile_gus 
{
	FILE* o;
	void* z;
} qfile_gut;

typedef struct qfile_us 
{
	qfile_gut file;
} qfile_ut;

typedef struct 
{
	qfile_ut handleFiles;
	qboolean handleSync;
	int	fileSize;
	int	zipFilePos;
	int	zipFileLen;
	qboolean zipFile;
	qboolean streamed;
	char name[256];
} fileHandleData_t;

struct customSnapshot
{
	int fileOffset = 0;
	int serverTime = 0;				// -> 0xF44898
	int landTime = 0;				// -> 0x9E6760

	int parseEntitiesNum = 0;		// -> 0xF6B294 - global entity index
	int parseClientsNum = 0;		// -> 0xF6B298 - global client index
	char snapshots[14816 * 32]{};	// -> 0xFAFABC - 32 snapshots
	char entities[284 * 2048]{};	// -> 0x106A6BC - 2048 entities
	char clients[148 * 2048]{};		// -> 0x10F86BC - 2048 clients

#ifdef enableChatRestoration
	int axisScore = 0;				// -> 0x9EE01C
	int alliesScore = 0;			// -> 0x9EE020
	int serverCommandSequence1 = 0;	// -> 0xB914D4 clc->serverCommandSequence
	int serverCommandSequence2 = 0;	// -> 0x98B714 cg_t->serverCommandSequence
	char chat[1968]{};				// -> 0x98C7A4
#endif
#ifdef enableGamestateRestoration
	char gameState[143300]{};		// -> 0xF48290
#endif
};

#endif