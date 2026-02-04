#ifndef __SEXYAPPFRAMEWORK_COMMON_H__
#define __SEXYAPPFRAMEWORK_COMMON_H__

#include <string>
#include <vector>
#include <map>
#include <cstdlib>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <type_traits>
#include <bit>

#ifdef _MSC_VER
#include <intrin.h>
#endif

#ifdef _WIN32
#undef _WIN32_WINNT
#undef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0500
#undef _UNICODE
#undef UNICODE
#define NOMINMAX 1
#include <windows.h>
#include <shellapi.h>
#include <mmsystem.h>
#else

#include <string.h>
#include <strings.h>
#include <stdint.h>

#endif

// fallback if NOMINMAX fails (somehow?)
#undef min
#undef max

// Define unreachable()
#ifdef _MSC_VER
#include <direct.h>
#include <io.h>
#define unreachable [](){ __assume(0); }
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define getcwd _getcwd
#define access _access
#define F_OK 0

inline int nanosleep(const struct timespec* ts, struct timespec* rem)
{
	// rem is not implemented
	HANDLE timer = CreateWaitableTimer(nullptr, TRUE, nullptr);
	if (!timer)
		return -1;

	// SetWaitableTimer() defines interval in 100ns units.
	// negative is to indicate relative time.
	time_t sec = ts->tv_sec;
	long nsec = ts->tv_nsec;
	if (sec < 0 || (sec == 0 && nsec <= 0))
	{
		CloseHandle(timer);
		return 0;
	}

	LARGE_INTEGER delay;
	delay.QuadPart = -((LONGLONG)sec * 10000000LL + (LONGLONG)nsec / 100LL);
	BOOL ok = SetWaitableTimer(timer, &delay, 0, nullptr, nullptr, FALSE) &&
		WaitForSingleObject(timer, INFINITE) == WAIT_OBJECT_0;

	CloseHandle(timer);

	if (!ok)
		return -1;

	return 0;
}
#else
#define unreachable __builtin_unreachable
#endif

#define LONG_BIGE_TO_NATIVE(l) (((l >> 24) & 0xFF) | ((l >> 8) & 0xFF00) | ((l << 8) & 0xFF0000) | ((l << 24) & 0xFF000000))
#define WORD_BIGE_TO_NATIVE(w) (((w >> 8) & 0xFF) | ((w << 8) & 0xFF00))
#define WORD_LITTLEE_TO_NATIVE(w) (w)

#define LENGTH(anyarray) (sizeof(anyarray) / sizeof(anyarray[0]))

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef int64_t int64;

typedef std::map<std::string, std::string> DefinesMap;
typedef std::vector<char> CharVector;

namespace Sexy
{

const ulong SEXY_RAND_MAX = 0x7FFFFFFF;

extern bool			gDebug;

#define printf(...) Sexy::PrintF(__VA_ARGS__)
void				PrintF(const char *text, ...);

int					Rand();
int					Rand(int range);
float				Rand(float range);
void				SRand(ulong theSeed);
extern std::string	VFormat(const char* fmt, va_list argPtr);
extern std::string	StrFormat(const char* fmt ...);
std::string			GetAppDataFolder();
void				SetAppDataFolder(const std::string& thePath);
std::string			GetAppDataPath(const std::string& theRelativePath);
const std::string&	GetResourceFolder();
void				SetResourceFolder(const std::string& thePath);
std::string			GetResourcePath(const std::string& theRelativePath);
std::string			StringToUpper(const std::string& theString);
std::string			StringToLower(const std::string& theString);
std::string			Upper(const std::string& theData);
std::string			Lower(const std::string& theData);
std::string			Trim(const std::string& theString);
bool				StringToInt(const std::string& theString, int* theIntVal);
bool				StringToDouble(const std::string& theString, double* theDoubleVal);
int					StrFindNoCase(const char *theStr, const char *theFind);
bool				StrPrefixNoCase(const char *theStr, const char *thePrefix, int maxLength = 10000000);
std::string			CommaSeperate(int theValue);
std::string			Evaluate(const std::string& theString, const DefinesMap& theDefinesMap);
std::string			XMLDecodeString(const std::string& theString);
std::string			XMLEncodeString(const std::string& theString);

bool				Deltree(const std::string& thePath);
bool				FileExists(const std::string& theFileName);
void				MkDir(const std::string& theDir);
std::string			GetFileName(const std::string& thePath, bool noExtension = false);
std::string			GetFileDir(const std::string& thePath, bool withSlash = false);
std::string			RemoveTrailingSlash(const std::string& theDirectory);
std::string			GetCurDir();
std::string			GetFullPath(const std::string& theRelPath);
std::string			GetPathFrom(const std::string& theRelPath, const std::string& theDir);
bool				AllowAllAccess(const std::string& theFileName);

// Read memory and then move the pointer
void				SMemR(void*& _Src, void* _Dst, size_t _Size);
void				SMemRStr(void*& _Src, std::string& theString);
// Write memory and then move the pointer
void				SMemW(void*& _Dst, const void* _Src, size_t _Size);
void				SMemWStr(void*& _Dst, const std::string& theString);

inline void			inlineLTrim(std::string &theData, const std::string& theChars = " \t\r\n")
{
    theData.erase(0, theData.find_first_not_of(theChars));
}

inline void			inlineRTrim(std::string &theData, const std::string& theChars = " \t\r\n")
{
    theData.resize(theData.find_last_not_of(theChars) + 1);
}

inline void			inlineTrim(std::string &theData, const std::string& theChars = " \t\r\n")
{
	inlineRTrim(theData, theChars);
	inlineLTrim(theData, theChars);
}

// UTF-8 path conversion helpers for Windows Unicode path support
#ifdef _WIN32
inline std::filesystem::path PathFromU8(const std::string& s)
{
	const auto* u8 = reinterpret_cast<const char8_t*>(s.data());
	return std::filesystem::path(std::u8string(u8, u8 + s.size()));
}

inline std::string PathToU8(const std::filesystem::path& p)
{
	auto u8 = p.generic_u8string();
	return std::string(u8.begin(), u8.end());
}
#else
inline std::filesystem::path PathFromU8(const std::string& s) { return std::filesystem::path(s); }
inline std::string PathToU8(const std::filesystem::path& p) { return p.string(); }
#endif

// Byte swap helpers
inline constexpr uint16_t ByteSwap16(uint16_t v) noexcept
{
	if (!std::is_constant_evaluated()) {
#if defined(__has_builtin)
#	if __has_builtin(__builtin_bswap16)
		return __builtin_bswap16(v);
#	endif
#elif defined(_MSC_VER)
		return _byteswap_ushort(v);
#endif
	}
	return (v >> 8) | (v << 8);
}

inline constexpr uint32_t ByteSwap32(uint32_t v) noexcept
{
	if (!std::is_constant_evaluated()) {
#if defined(__has_builtin)
#	if __has_builtin(__builtin_bswap32)
		return __builtin_bswap32(v);
#	endif
#elif defined(_MSC_VER)
		return _byteswap_ulong(v);
#endif
	}
	return ((v & 0x000000FFu) << 24) |
	       ((v & 0x0000FF00u) <<  8) |
	       ((v & 0x00FF0000u) >>  8) |
	       ((v & 0xFF000000u) >> 24);
}

inline constexpr uint64_t ByteSwap64(uint64_t v) noexcept
{
	if (!std::is_constant_evaluated()) {
#if defined(__has_builtin)
#	if __has_builtin(__builtin_bswap64)
		return __builtin_bswap64(v);
#	endif
#elif defined(_MSC_VER)
		return _byteswap_uint64(v);
#endif
	}
	return ((v & 0x00000000000000FFULL) << 56) |
	       ((v & 0x000000000000FF00ULL) << 40) |
	       ((v & 0x0000000000FF0000ULL) << 24) |
	       ((v & 0x00000000FF000000ULL) <<  8) |
	       ((v & 0x000000FF00000000ULL) >>  8) |
	       ((v & 0x0000FF0000000000ULL) >> 24) |
	       ((v & 0x00FF000000000000ULL) >> 40) |
	       ((v & 0xFF00000000000000ULL) >> 56);
}

// Little-endian conversion helpers (file format is always little-endian)
template <typename T>
inline constexpr T LEConvert(T v) noexcept
{
	if constexpr (std::endian::native == std::endian::big)
	{
		if constexpr (sizeof(T) == 2)
			return static_cast<T>(ByteSwap16(static_cast<uint16_t>(v)));
		if constexpr (sizeof(T) == 4)
			return static_cast<T>(ByteSwap32(static_cast<uint32_t>(v)));
		if constexpr (sizeof(T) == 8)
			return static_cast<T>(ByteSwap64(static_cast<uint64_t>(v)));
	}
	return v;
}

inline constexpr uint16_t FromLE16(uint16_t v) noexcept { return LEConvert(v); }
inline constexpr uint16_t ToLE16(uint16_t v) noexcept { return LEConvert(v); }
inline constexpr uint32_t FromLE32(uint32_t v) noexcept { return LEConvert(v); }
inline constexpr uint32_t ToLE32(uint32_t v) noexcept { return LEConvert(v); }
inline constexpr uint64_t FromLE64(uint64_t v) noexcept { return LEConvert(v); }
inline constexpr uint64_t ToLE64(uint64_t v) noexcept { return LEConvert(v); }

struct StringLessNoCase { bool operator()(const std::string &s1, const std::string &s2) const { return strcasecmp(s1.c_str(),s2.c_str())<0; } };

}

#endif //__SEXYAPPFRAMEWORK_COMMON_H__
