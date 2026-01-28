#include "Music.h"
#include "SaveGame.h"
#include "../Board.h"
#include "../Challenge.h"
#include "../SeedPacket.h"
#include "../../LawnApp.h"
#include "../CursorObject.h"
#include "../../Resources.h"
#include "../../ConstEnums.h"
#include "../MessageWidget.h"
#include "../../Sexy.TodLib/Trail.h"
#include "zlib.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/DataArray.h"
#include "../../Sexy.TodLib/TodList.h"
#include "DataSync.h"
#include <vector>

static const char* FILE_COMPILE_TIME_STRING = "Jul  2 201011:47:03"; // The compile time of 1.2.0.1073 GOTY
//static const char* FILE_COMPILE_TIME_STRING = "Dec 10 201014:56:46" // The compile time of 1.2.0.1096 GOTY Steam
static const unsigned int SAVE_FILE_MAGIC_NUMBER = 0xFEEDDEAD;
static const unsigned int SAVE_FILE_VERSION = 2U;
static unsigned int SAVE_FILE_DATE = crc32(0, (Bytef*)FILE_COMPILE_TIME_STRING, strlen(FILE_COMPILE_TIME_STRING));  //[0x6AA7EC]

static const char SAVE_FILE_MAGIC_V4[12] = "PVZP_SAVE4";
static const unsigned int SAVE_FILE_V4_VERSION = 1U;

struct SaveFileHeaderV4
{
	char			mMagic[12];
	unsigned int	mVersion;
	unsigned int	mPayloadSize;
	unsigned int	mPayloadCrc;
};

enum SaveChunkTypeV4
{
	SAVE4_CHUNK_BOARD_BASE = 1,
	SAVE4_CHUNK_ZOMBIES = 2,
	SAVE4_CHUNK_PLANTS = 3,
	SAVE4_CHUNK_PROJECTILES = 4,
	SAVE4_CHUNK_COINS = 5,
	SAVE4_CHUNK_MOWERS = 6,
	SAVE4_CHUNK_GRIDITEMS = 7,
	SAVE4_CHUNK_PARTICLE_EMITTERS = 8,
	SAVE4_CHUNK_PARTICLE_PARTICLES = 9,
	SAVE4_CHUNK_PARTICLE_SYSTEMS = 10,
	SAVE4_CHUNK_REANIMATIONS = 11,
	SAVE4_CHUNK_TRAILS = 12,
	SAVE4_CHUNK_ATTACHMENTS = 13,
	SAVE4_CHUNK_CURSOR = 14,
	SAVE4_CHUNK_CURSOR_PREVIEW = 15,
	SAVE4_CHUNK_ADVICE = 16,
	SAVE4_CHUNK_SEEDBANK = 17,
	SAVE4_CHUNK_SEEDPACKETS = 18,
	SAVE4_CHUNK_CHALLENGE = 19,
	SAVE4_CHUNK_MUSIC = 20
};

static const unsigned int SAVE4_CHUNK_VERSION = 1U;

static void AppendU32LE(std::vector<unsigned char>& theOut, uint32_t theValue)
{
	unsigned char aBytes[4];
	aBytes[0] = (unsigned char)(theValue & 0xFF);
	aBytes[1] = (unsigned char)((theValue >> 8) & 0xFF);
	aBytes[2] = (unsigned char)((theValue >> 16) & 0xFF);
	aBytes[3] = (unsigned char)((theValue >> 24) & 0xFF);
	theOut.insert(theOut.end(), aBytes, aBytes + 4);
}

static void AppendBytes(std::vector<unsigned char>& theOut, const void* theData, size_t theLen)
{
	const unsigned char* aBytes = (const unsigned char*)theData;
	theOut.insert(theOut.end(), aBytes, aBytes + theLen);
}

static void AppendChunk(std::vector<unsigned char>& theOut, uint32_t theChunkType, const std::vector<unsigned char>& theChunkData)
{
	AppendU32LE(theOut, theChunkType);
	AppendU32LE(theOut, (uint32_t)theChunkData.size());
	AppendBytes(theOut, theChunkData.data(), theChunkData.size());
}

class TLVReader
{
public:
	const unsigned char* mData;
	size_t mSize;
	size_t mPos;
	bool mOk;

public:
	TLVReader(const unsigned char* theData, size_t theSize)
		: mData(theData), mSize(theSize), mPos(0), mOk(true)
	{
	}

	bool ReadU32(uint32_t& theValue)
	{
		if (mPos + 4 > mSize)
		{
			mOk = false;
			theValue = 0;
			return false;
		}
		theValue = (uint32_t)mData[mPos] | ((uint32_t)mData[mPos + 1] << 8) | ((uint32_t)mData[mPos + 2] << 16) | ((uint32_t)mData[mPos + 3] << 24);
		mPos += 4;
		return true;
	}

	bool ReadBytes(const unsigned char*& thePtr, size_t theLen)
	{
		if (mPos + theLen > mSize)
		{
			mOk = false;
			thePtr = nullptr;
			return false;
		}
		thePtr = mData + mPos;
		mPos += theLen;
		return true;
	}
};

class PortableSaveContext
{
public:
	bool		mReading = false;
	bool		mFailed = false;
	DataReader*	mReader = nullptr;
	DataWriter*	mWriter = nullptr;

public:
	explicit PortableSaveContext(DataReader& theReader)
	{
		mReading = true;
		mReader = &theReader;
	}

	explicit PortableSaveContext(DataWriter& theWriter)
	{
		mReading = false;
		mWriter = &theWriter;
	}

	void SyncBytes(void* theData, uint32_t theDataLen)
	{
		try
		{
			if (mReading)
			{
				mReader->ReadBytes(theData, theDataLen);
			}
			else
			{
				mWriter->WriteBytes(theData, theDataLen);
			}
		}
		catch (DataReaderException&)
		{
			mFailed = true;
			if (mReading)
				memset(theData, 0, theDataLen);
		}
	}

	void SyncBool(bool& theBool)
	{
		if (mReading)
		{
			try
			{
				theBool = mReader->ReadBool();
			}
			catch (DataReaderException&)
			{
				mFailed = true;
				theBool = false;
			}
		}
		else
		{
			mWriter->WriteBool(theBool);
		}
	}

	void SyncUInt32(uint32_t& theValue)
	{
		if (mReading)
		{
			try
			{
				theValue = mReader->ReadLong();
			}
			catch (DataReaderException&)
			{
				mFailed = true;
				theValue = 0;
			}
		}
		else
		{
			mWriter->WriteLong(theValue);
		}
	}

	void SyncInt32(int& theValue)
	{
		if (mReading)
		{
			try
			{
				theValue = (int)mReader->ReadLong();
			}
			catch (DataReaderException&)
			{
				mFailed = true;
				theValue = 0;
			}
		}
		else
		{
			mWriter->WriteLong((uint32_t)theValue);
		}
	}

	void SyncFloat(float& theValue)
	{
		if (mReading)
		{
			try
			{
				theValue = mReader->ReadFloat();
			}
			catch (DataReaderException&)
			{
				mFailed = true;
				theValue = 0.0f;
			}
		}
		else
		{
			mWriter->WriteFloat(theValue);
		}
	}

	void SyncUInt64(uint64_t& theValue)
	{
		uint32_t aLow = (uint32_t)(theValue & 0xFFFFFFFFULL);
		uint32_t aHigh = (uint32_t)((theValue >> 32) & 0xFFFFFFFFULL);
		SyncUInt32(aLow);
		SyncUInt32(aHigh);
		if (mReading)
			theValue = ((uint64_t)aHigh << 32) | aLow;
	}

	void SyncInt64(int64_t& theValue)
	{
		uint64_t aValue = (uint64_t)theValue;
		SyncUInt64(aValue);
		if (mReading)
			theValue = (int64_t)aValue;
	}

	template <typename TEnum>
	void SyncEnum(TEnum& theEnum)
	{
		int aValue = (int)theEnum;
		SyncInt32(aValue);
		if (mReading)
			theEnum = (TEnum)aValue;
	}
};


template <typename TObject, typename TField>
static void SyncPodTail(PortableSaveContext& theContext, TObject& theObject, TField TObject::* theFirstField)
{
	unsigned char* aStart = (unsigned char*)&theObject;
	unsigned char* aField = (unsigned char*)&(theObject.*theFirstField);
	size_t aOffset = (size_t)(aField - aStart);
	if (aOffset < sizeof(TObject))
	{
		theContext.SyncBytes(aStart + aOffset, (uint32_t)(sizeof(TObject) - aOffset));
	}
}

static void SyncColorPortable(PortableSaveContext& theContext, Color& theColor)
{
	theContext.SyncBytes(&theColor, sizeof(theColor));
}

static void SyncVector2Portable(PortableSaveContext& theContext, SexyVector2& theVector)
{
	theContext.SyncBytes(&theVector, sizeof(theVector));
}

static void SyncMatrixPortable(PortableSaveContext& theContext, SexyMatrix3& theMatrix)
{
	theContext.SyncBytes(&theMatrix, sizeof(theMatrix));
}

static void SyncRectPortable(PortableSaveContext& theContext, Rect& theRect)
{
	theContext.SyncBytes(&theRect, sizeof(theRect));
}

static void SyncReanimationDefPortable(PortableSaveContext& theContext, ReanimatorDefinition*& theDefinition)
{
	if (theContext.mReading)
	{
		int aReanimType = 0;
		theContext.SyncInt32(aReanimType);
		if (aReanimType == (int)ReanimationType::REANIM_NONE)
		{
			theDefinition = nullptr;
		}
		else if (aReanimType >= 0 && aReanimType < (int)ReanimationType::NUM_REANIMS)
		{
			ReanimatorEnsureDefinitionLoaded((ReanimationType)aReanimType, true);
			theDefinition = &gReanimatorDefArray[aReanimType];
		}
		else
		{
			theContext.mFailed = true;
		}
	}
	else
	{
		int aReanimType = (int)ReanimationType::REANIM_NONE;
		for (int i = 0; i < (int)ReanimationType::NUM_REANIMS; i++)
		{
			ReanimatorDefinition* aDef = &gReanimatorDefArray[i];
			if (theDefinition == aDef)
			{
				aReanimType = i;
				break;
			}
		}
		theContext.SyncInt32(aReanimType);
	}
}

static void SyncParticleDefPortable(PortableSaveContext& theContext, TodParticleDefinition*& theDefinition)
{
	if (theContext.mReading)
	{
		int aParticleType = 0;
		theContext.SyncInt32(aParticleType);
		if (aParticleType == (int)ParticleEffect::PARTICLE_NONE)
		{
			theDefinition = nullptr;
		}
		else if (aParticleType >= 0 && aParticleType < (int)ParticleEffect::NUM_PARTICLES)
		{
			theDefinition = &gParticleDefArray[aParticleType];
		}
		else
		{
			theContext.mFailed = true;
		}
	}
	else
	{
		int aParticleType = (int)ParticleEffect::PARTICLE_NONE;
		for (int i = 0; i < (int)ParticleEffect::NUM_PARTICLES; i++)
		{
			TodParticleDefinition* aDef = &gParticleDefArray[i];
			if (theDefinition == aDef)
			{
				aParticleType = i;
				break;
			}
		}
		theContext.SyncInt32(aParticleType);
	}
}

static void SyncTrailDefPortable(PortableSaveContext& theContext, TrailDefinition*& theDefinition)
{
	if (theContext.mReading)
	{
		int aTrailType = 0;
		theContext.SyncInt32(aTrailType);
		if (aTrailType == TrailType::TRAIL_NONE)
		{
			theDefinition = nullptr;
		}
		else if (aTrailType >= 0 && aTrailType < TrailType::NUM_TRAILS)
		{
			theDefinition = &gTrailDefArray[aTrailType];
		}
		else
		{
			theContext.mFailed = true;
		}
	}
	else
	{
		int aTrailType = TrailType::TRAIL_NONE;
		for (int i = 0; i < TrailType::NUM_TRAILS; i++)
		{
			TrailDefinition* aDef = &gTrailDefArray[i];
			if (theDefinition == aDef)
			{
				aTrailType = i;
				break;
			}
		}
		theContext.SyncInt32(aTrailType);
	}
}

static void SyncImagePortable(PortableSaveContext& theContext, Image*& theImage)
{
	if (theContext.mReading)
	{
		ResourceId aResID;
		theContext.SyncInt32((int&)aResID);
		if (aResID == Sexy::ResourceId::RESOURCE_ID_MAX)
		{
			theImage = nullptr;
		}
		else
		{
			theImage = GetImageById(aResID);
		}
	}
	else
	{
		ResourceId aResID;
		if (theImage != nullptr)
		{
			aResID = GetIdByImage(theImage);
		}
		else
		{
			aResID = Sexy::ResourceId::RESOURCE_ID_MAX;
		}
		theContext.SyncInt32((int&)aResID);
	}
}

static void SyncDataIDListPortable(TodList<unsigned int>* theDataIDList, PortableSaveContext& theContext, TodAllocator* theAllocator)
{
	try
	{
		if (theContext.mReading)
		{
			if (theDataIDList)
			{
				theDataIDList->mHead = nullptr;
				theDataIDList->mTail = nullptr;
				theDataIDList->mSize = 0;
				theDataIDList->SetAllocator(theAllocator);
			}

			int aCount = 0;
			theContext.SyncInt32(aCount);
			for (int i = 0; i < aCount; i++)
			{
				unsigned int aDataID = 0;
				theContext.SyncBytes(&aDataID, sizeof(aDataID));
				theDataIDList->AddTail(aDataID);
			}
		}
		else
		{
			int aCount = theDataIDList->mSize;
			theContext.SyncInt32(aCount);
			for (TodListNode<unsigned int>* aNode = theDataIDList->mHead; aNode != nullptr; aNode = aNode->mNext)
			{
				unsigned int aDataID = aNode->mValue;
				theContext.SyncBytes(&aDataID, sizeof(aDataID));
			}
		}
	}
	catch (std::exception&)
	{
		return;
	}
}

static void SyncGameObjectPortable(PortableSaveContext& theContext, GameObject& theObject)
{
	theContext.SyncInt32(theObject.mX);
	theContext.SyncInt32(theObject.mY);
	theContext.SyncInt32(theObject.mWidth);
	theContext.SyncInt32(theObject.mHeight);
	theContext.SyncBool(theObject.mVisible);
	theContext.SyncInt32(theObject.mRow);
	theContext.SyncInt32(theObject.mRenderOrder);
}

template <typename TWriterFn>
static void AppendFieldWithSync(std::vector<unsigned char>& theOut, uint32_t theFieldId, TWriterFn theWriterFn)
{
	DataWriter aWriter;
	aWriter.OpenMemory(0x100);
	PortableSaveContext aContext(aWriter);
	theWriterFn(aContext);
	if (aContext.mFailed)
		return;

	std::vector<unsigned char> aFieldData;
	aFieldData.resize(aWriter.GetDataLen());
	memcpy(aFieldData.data(), aWriter.GetDataPtr(), aWriter.GetDataLen());
	AppendU32LE(theOut, theFieldId);
	AppendU32LE(theOut, (uint32_t)aFieldData.size());
	AppendBytes(theOut, aFieldData.data(), aFieldData.size());
}

template <typename TReaderFn>
static bool ApplyFieldWithSync(const unsigned char* theData, size_t theSize, TReaderFn theReaderFn)
{
	DataReader aReader;
	aReader.OpenMemory(theData, (uint32_t)theSize, false);
	PortableSaveContext aContext(aReader);
	theReaderFn(aContext);
	return !aContext.mFailed;
}

static void WriteGameObjectField(std::vector<unsigned char>& theOut, uint32_t theFieldId, GameObject& theObject)
{
	AppendFieldWithSync(theOut, theFieldId, [&](PortableSaveContext& aContext)
	{
		SyncGameObjectPortable(aContext, theObject);
	});
}

static bool ReadGameObjectField(const unsigned char* theData, size_t theSize, GameObject& theObject)
{
	return ApplyFieldWithSync(theData, theSize, [&](PortableSaveContext& aContext)
	{
		SyncGameObjectPortable(aContext, theObject);
	});
}

template <typename TObject, typename TField>
static void WritePodTailField(std::vector<unsigned char>& theOut, uint32_t theFieldId, TObject& theObject, TField TObject::* theFirstField)
{
	AppendFieldWithSync(theOut, theFieldId, [&](PortableSaveContext& aContext)
	{
		SyncPodTail(aContext, theObject, theFirstField);
	});
}

template <typename TObject, typename TField>
static bool ReadPodTailField(const unsigned char* theData, size_t theSize, TObject& theObject, TField TObject::* theFirstField)
{
	return ApplyFieldWithSync(theData, theSize, [&](PortableSaveContext& aContext)
	{
		SyncPodTail(aContext, theObject, theFirstField);
	});
}

static void WriteTLVBlob(PortableSaveContext& theContext, const std::vector<unsigned char>& theBlob)
{
	uint32_t aSize = (uint32_t)theBlob.size();
	theContext.SyncUInt32(aSize);
	if (aSize > 0)
		theContext.SyncBytes((void*)theBlob.data(), aSize);
}

static bool ReadTLVBlob(PortableSaveContext& theContext, std::vector<unsigned char>& theBlob)
{
	uint32_t aSize = 0;
	theContext.SyncUInt32(aSize);
	if (theContext.mFailed)
		return false;
	theBlob.resize(aSize);
	if (aSize > 0)
		theContext.SyncBytes(theBlob.data(), aSize);
	return !theContext.mFailed;
}

static void SyncReanimTransformPortable(PortableSaveContext& theContext, ReanimatorTransform& theTransform)
{
	theContext.SyncFloat(theTransform.mTransX);
	theContext.SyncFloat(theTransform.mTransY);
	theContext.SyncFloat(theTransform.mSkewX);
	theContext.SyncFloat(theTransform.mSkewY);
	theContext.SyncFloat(theTransform.mScaleX);
	theContext.SyncFloat(theTransform.mScaleY);
	theContext.SyncFloat(theTransform.mFrame);
	theContext.SyncFloat(theTransform.mAlpha);
	if (theContext.mReading)
	{
		theTransform.mImage = nullptr;
		theTransform.mFont = nullptr;
		theTransform.mText = "";
	}
}

static void SyncReanimTrackInstancePortable(PortableSaveContext& theContext, ReanimatorTrackInstance& theTrackInstance)
{
	theContext.SyncInt32(theTrackInstance.mBlendCounter);
	theContext.SyncInt32(theTrackInstance.mBlendTime);
	SyncReanimTransformPortable(theContext, theTrackInstance.mBlendTransform);
	theContext.SyncFloat(theTrackInstance.mShakeOverride);
	theContext.SyncFloat(theTrackInstance.mShakeX);
	theContext.SyncFloat(theTrackInstance.mShakeY);
	theContext.SyncInt32((int&)theTrackInstance.mAttachmentID);
	SyncImagePortable(theContext, theTrackInstance.mImageOverride);
	theContext.SyncInt32(theTrackInstance.mRenderGroup);
	SyncColorPortable(theContext, theTrackInstance.mTrackColor);
	theContext.SyncBool(theTrackInstance.mIgnoreClipRect);
	theContext.SyncBool(theTrackInstance.mTruncateDisappearingFrames);
	theContext.SyncBool(theTrackInstance.mIgnoreColorOverride);
	theContext.SyncBool(theTrackInstance.mIgnoreExtraAdditiveColor);
}

static void SyncReanimationPortable(Board* theBoard, Reanimation* theReanimation, PortableSaveContext& theContext)
{
	SyncReanimationDefPortable(theContext, theReanimation->mDefinition);
	if (theContext.mReading)
	{
		theReanimation->mReanimationHolder = theBoard->mApp->mEffectSystem->mReanimationHolder;
	}

	ReanimatorDefinition* aDef = theReanimation->mDefinition;
	ReanimatorDefinition* aDefStart = gReanimatorDefArray;
	ReanimatorDefinition* aDefEnd = gReanimatorDefArray + (int)ReanimationType::NUM_REANIMS;
	if (aDef == nullptr || aDef < aDefStart || aDef >= aDefEnd)
	{
		int aType = (int)theReanimation->mReanimationType;
		if (aType >= 0 && aType < (int)ReanimationType::NUM_REANIMS)
		{
			ReanimatorEnsureDefinitionLoaded((ReanimationType)aType, true);
			aDef = &gReanimatorDefArray[aType];
			if (theContext.mReading)
				theReanimation->mDefinition = aDef;
		}
		else
		{
			aDef = nullptr;
		}
	}

	theContext.SyncEnum(theReanimation->mReanimationType);
	theContext.SyncFloat(theReanimation->mAnimTime);
	theContext.SyncFloat(theReanimation->mAnimRate);
	theContext.SyncEnum(theReanimation->mLoopType);
	theContext.SyncBool(theReanimation->mDead);
	theContext.SyncInt32(theReanimation->mFrameStart);
	theContext.SyncInt32(theReanimation->mFrameCount);
	theContext.SyncInt32(theReanimation->mFrameBasePose);
	SyncMatrixPortable(theContext, theReanimation->mOverlayMatrix);
	SyncColorPortable(theContext, theReanimation->mColorOverride);
	theContext.SyncInt32(theReanimation->mLoopCount);
	theContext.SyncBool(theReanimation->mIsAttachment);
	theContext.SyncInt32(theReanimation->mRenderOrder);
	SyncColorPortable(theContext, theReanimation->mExtraAdditiveColor);
	theContext.SyncBool(theReanimation->mEnableExtraAdditiveDraw);
	SyncColorPortable(theContext, theReanimation->mExtraOverlayColor);
	theContext.SyncBool(theReanimation->mEnableExtraOverlayDraw);
	theContext.SyncFloat(theReanimation->mLastFrameTime);
	theContext.SyncEnum(theReanimation->mFilterEffect);

	if (aDef && aDef->mTracks.count != 0)
	{
		int aCount = aDef->mTracks.count;
		bool aUseTemp = (theReanimation->mTrackInstances == nullptr);
		if (theContext.mReading)
		{
			theReanimation->mTrackInstances = (ReanimatorTrackInstance*)FindGlobalAllocator(aCount * sizeof(ReanimatorTrackInstance))->Calloc(aCount * sizeof(ReanimatorTrackInstance));
			if (theReanimation->mTrackInstances == nullptr)
			{
				aUseTemp = true;
			}
			else
			{
				aUseTemp = false;
			}
		}
		else
		{
			TodAllocator* aAllocator = FindGlobalAllocator(aCount * sizeof(ReanimatorTrackInstance));
			if (aAllocator == nullptr || !aAllocator->IsPointerFromAllocator(theReanimation->mTrackInstances) || aAllocator->IsPointerOnFreeList(theReanimation->mTrackInstances))
			{
				aUseTemp = true;
			}
		}
		if (aUseTemp)
		{
			std::vector<ReanimatorTrackInstance> aTemp;
			aTemp.resize(aCount);
			memset(aTemp.data(), 0, sizeof(ReanimatorTrackInstance) * aCount);
			for (int aTrackIndex = 0; aTrackIndex < aCount; aTrackIndex++)
			{
				SyncReanimTrackInstancePortable(theContext, aTemp[aTrackIndex]);
			}
		}
		else
		{
			for (int aTrackIndex = 0; aTrackIndex < aCount; aTrackIndex++)
			{
				SyncReanimTrackInstancePortable(theContext, theReanimation->mTrackInstances[aTrackIndex]);
			}
		}
	}
}

static void SyncParticlePortable(TodParticle* theParticle, PortableSaveContext& theContext)
{
	theContext.SyncInt32(theParticle->mParticleDuration);
	theContext.SyncInt32(theParticle->mParticleAge);
	theContext.SyncFloat(theParticle->mParticleTimeValue);
	theContext.SyncFloat(theParticle->mParticleLastTimeValue);
	theContext.SyncFloat(theParticle->mAnimationTimeValue);
	SyncVector2Portable(theContext, theParticle->mVelocity);
	SyncVector2Portable(theContext, theParticle->mPosition);
	theContext.SyncInt32(theParticle->mImageFrame);
	theContext.SyncFloat(theParticle->mSpinPosition);
	theContext.SyncFloat(theParticle->mSpinVelocity);
	theContext.SyncInt32((int&)theParticle->mCrossFadeParticleID);
	theContext.SyncInt32(theParticle->mCrossFadeDuration);
	for (int i = 0; i < ParticleTracks::NUM_PARTICLE_TRACKS; i++)
		theContext.SyncFloat(theParticle->mParticleInterp[i]);
	for (int i = 0; i < MAX_PARTICLE_FIELDS; i++)
	{
		for (int j = 0; j < 2; j++)
			theContext.SyncFloat(theParticle->mParticleFieldInterp[i][j]);
	}
}

static void SyncParticleEmitterPortable(TodParticleSystem* theParticleSystem, TodParticleEmitter* theParticleEmitter, PortableSaveContext& theContext)
{
	int aEmitterDefIndex = 0;
	if (theContext.mReading)
	{
		theContext.SyncInt32(aEmitterDefIndex);
		theParticleEmitter->mParticleSystem = theParticleSystem;
		theParticleEmitter->mEmitterDef = &theParticleSystem->mParticleDef->mEmitterDefs[aEmitterDefIndex];
	}
	else
	{
		aEmitterDefIndex = ((intptr_t)theParticleEmitter->mEmitterDef - (intptr_t)theParticleSystem->mParticleDef->mEmitterDefs) / sizeof(TodEmitterDefinition);
		theContext.SyncInt32(aEmitterDefIndex);
	}

	SyncDataIDListPortable((TodList<unsigned int>*)&theParticleEmitter->mParticleList, theContext, &theParticleSystem->mParticleHolder->mParticleListNodeAllocator);
	SyncVector2Portable(theContext, theParticleEmitter->mSystemCenter);
	SyncColorPortable(theContext, theParticleEmitter->mColorOverride);
	SyncImagePortable(theContext, theParticleEmitter->mImageOverride);
	theContext.SyncFloat(theParticleEmitter->mSpawnAccum);
	theContext.SyncInt32(theParticleEmitter->mParticlesSpawned);
	theContext.SyncInt32(theParticleEmitter->mSystemAge);
	theContext.SyncInt32(theParticleEmitter->mSystemDuration);
	theContext.SyncFloat(theParticleEmitter->mSystemTimeValue);
	theContext.SyncFloat(theParticleEmitter->mSystemLastTimeValue);
	theContext.SyncBool(theParticleEmitter->mDead);
	theContext.SyncBool(theParticleEmitter->mExtraAdditiveDrawOverride);
	theContext.SyncFloat(theParticleEmitter->mScaleOverride);
	theContext.SyncInt32((int&)theParticleEmitter->mCrossFadeEmitterID);
	theContext.SyncInt32(theParticleEmitter->mEmitterCrossFadeCountDown);
	theContext.SyncInt32(theParticleEmitter->mFrameOverride);
	for (int i = 0; i < ParticleSystemTracks::NUM_SYSTEM_TRACKS; i++)
		theContext.SyncFloat(theParticleEmitter->mTrackInterp[i]);
	for (int i = 0; i < MAX_PARTICLE_FIELDS; i++)
	{
		for (int j = 0; j < 2; j++)
			theContext.SyncFloat(theParticleEmitter->mSystemFieldInterp[i][j]);
	}

	for (TodListNode<ParticleID>* aNode = theParticleEmitter->mParticleList.mHead; aNode != nullptr; aNode = aNode->mNext)
	{
		TodParticle* aParticle = theParticleSystem->mParticleHolder->mParticles.DataArrayGet((unsigned int)aNode->mValue);
		if (theContext.mReading)
		{
			aParticle->mParticleEmitter = theParticleEmitter;
		}
		SyncParticlePortable(aParticle, theContext);
	}
}

static void SyncParticleSystemPortable(Board* theBoard, TodParticleSystem* theParticleSystem, PortableSaveContext& theContext)
{
	SyncParticleDefPortable(theContext, theParticleSystem->mParticleDef);
	if (theContext.mReading)
	{
		theParticleSystem->mParticleHolder = theBoard->mApp->mEffectSystem->mParticleHolder;
	}

	SyncDataIDListPortable((TodList<unsigned int>*)&theParticleSystem->mEmitterList, theContext, &theParticleSystem->mParticleHolder->mEmitterListNodeAllocator);
	for (TodListNode<ParticleEmitterID>* aNode = theParticleSystem->mEmitterList.mHead; aNode != nullptr; aNode = aNode->mNext)
	{
		TodParticleEmitter* aEmitter = theParticleSystem->mParticleHolder->mEmitters.DataArrayGet((unsigned int)aNode->mValue);
		SyncParticleEmitterPortable(theParticleSystem, aEmitter, theContext);
	}

	theContext.SyncEnum(theParticleSystem->mEffectType);
	theContext.SyncBool(theParticleSystem->mDead);
	theContext.SyncBool(theParticleSystem->mIsAttachment);
	theContext.SyncInt32(theParticleSystem->mRenderOrder);
	theContext.SyncBool(theParticleSystem->mDontUpdate);
}

static void SyncTrailPortable(Board* theBoard, Trail* theTrail, PortableSaveContext& theContext)
{
	SyncTrailDefPortable(theContext, theTrail->mDefinition);
	if (theContext.mReading)
	{
		theTrail->mTrailHolder = theBoard->mApp->mEffectSystem->mTrailHolder;
	}

	for (int i = 0; i < 20; i++)
		SyncVector2Portable(theContext, theTrail->mTrailPoints[i].aPos);
	theContext.SyncInt32(theTrail->mNumTrailPoints);
	theContext.SyncBool(theTrail->mDead);
	theContext.SyncInt32(theTrail->mRenderOrder);
	theContext.SyncInt32(theTrail->mTrailAge);
	theContext.SyncInt32(theTrail->mTrailDuration);
	for (int i = 0; i < 4; i++)
		theContext.SyncFloat(theTrail->mTrailInterp[i]);
	SyncVector2Portable(theContext, theTrail->mTrailCenter);
	theContext.SyncBool(theTrail->mIsAttachment);
	SyncColorPortable(theContext, theTrail->mColorOverride);
}

template <typename T, typename TSyncFn>
static void SyncDataArrayPortable(PortableSaveContext& theContext, DataArray<T>& theDataArray, TSyncFn theSyncFn)
{
	theContext.SyncUInt32(theDataArray.mFreeListHead);
	theContext.SyncUInt32(theDataArray.mMaxUsedCount);
	theContext.SyncUInt32(theDataArray.mSize);
	theContext.SyncUInt32(theDataArray.mNextKey);
	uint32_t aMaxSize = theDataArray.mMaxSize;
	theContext.SyncUInt32(aMaxSize);
	if (theContext.mReading && aMaxSize != theDataArray.mMaxSize)
	{
		theContext.mFailed = true;
	}

	for (unsigned int i = 0; i < theDataArray.mMaxUsedCount; i++)
	{
		theContext.SyncUInt32(theDataArray.mBlock[i].mID);
		theSyncFn(theDataArray.mBlock[i].mItem);
	}
}

template <typename T>
static void SyncDataArrayIdsOnlyPortable(PortableSaveContext& theContext, DataArray<T>& theDataArray)
{
	theContext.SyncUInt32(theDataArray.mFreeListHead);
	theContext.SyncUInt32(theDataArray.mMaxUsedCount);
	theContext.SyncUInt32(theDataArray.mSize);
	theContext.SyncUInt32(theDataArray.mNextKey);
	uint32_t aMaxSize = theDataArray.mMaxSize;
	theContext.SyncUInt32(aMaxSize);
	if (theContext.mReading && aMaxSize != theDataArray.mMaxSize)
	{
		theContext.mFailed = true;
	}

	for (unsigned int i = 0; i < theDataArray.mMaxUsedCount; i++)
	{
		theContext.SyncUInt32(theDataArray.mBlock[i].mID);
	}
}

template <typename T, typename TWriteFn, typename TReadFn>
static void SyncDataArrayPortableTLV(PortableSaveContext& theContext, DataArray<T>& theDataArray, TWriteFn theWriteFn, TReadFn theReadFn)
{
	theContext.SyncUInt32(theDataArray.mFreeListHead);
	theContext.SyncUInt32(theDataArray.mMaxUsedCount);
	theContext.SyncUInt32(theDataArray.mSize);
	theContext.SyncUInt32(theDataArray.mNextKey);
	uint32_t aMaxSize = theDataArray.mMaxSize;
	theContext.SyncUInt32(aMaxSize);
	if (theContext.mReading && aMaxSize != theDataArray.mMaxSize)
	{
		theContext.mFailed = true;
	}

	for (unsigned int i = 0; i < theDataArray.mMaxUsedCount; i++)
	{
		theContext.SyncUInt32(theDataArray.mBlock[i].mID);
		if (theContext.mReading)
		{
			uint32_t aItemSize = 0;
			theContext.SyncUInt32(aItemSize);
			memset(&theDataArray.mBlock[i].mItem, 0, sizeof(T));
			std::vector<unsigned char> aItemData;
			aItemData.resize(aItemSize);
			if (aItemSize > 0)
				theContext.SyncBytes(aItemData.data(), aItemSize);
			TLVReader aReader(aItemData.data(), aItemSize);
			while (aReader.mOk && aReader.mPos < aReader.mSize)
			{
				uint32_t aFieldId = 0;
				uint32_t aFieldSize = 0;
				if (!aReader.ReadU32(aFieldId) || !aReader.ReadU32(aFieldSize))
					break;
				const unsigned char* aFieldData = nullptr;
				if (!aReader.ReadBytes(aFieldData, aFieldSize))
					break;
				theReadFn(aFieldId, aFieldData, aFieldSize, theDataArray.mBlock[i].mItem);
			}
		}
		else
		{
			bool aActive = (theDataArray.mBlock[i].mID & DATA_ARRAY_KEY_MASK) != 0;
			uint32_t aItemSize = 0;
			std::vector<unsigned char> aItemData;
			if (aActive)
			{
				theWriteFn(aItemData, theDataArray.mBlock[i].mItem);
				aItemSize = (uint32_t)aItemData.size();
			}
			theContext.SyncUInt32(aItemSize);
			if (aItemSize > 0)
				theContext.SyncBytes(aItemData.data(), aItemSize);
		}
	}
}

static void SyncBoardBasePortable(PortableSaveContext& theContext, Board* theBoard)
{
	if (theContext.mReading)
	{
		std::vector<unsigned char> aBlob;
		if (!ReadTLVBlob(theContext, aBlob))
			return;
		TLVReader aReader(aBlob.data(), aBlob.size());
		while (aReader.mOk && aReader.mPos < aReader.mSize)
		{
			uint32_t aFieldId = 0;
			uint32_t aFieldSize = 0;
			if (!aReader.ReadU32(aFieldId) || !aReader.ReadU32(aFieldSize))
				break;
			const unsigned char* aFieldData = nullptr;
			if (!aReader.ReadBytes(aFieldData, aFieldSize))
				break;

			switch (aFieldId)
			{
			case 1U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mPaused); }); break;
			case 2U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mGridSquareType, sizeof(theBoard->mGridSquareType)); }); break;
			case 3U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mGridCelLook, sizeof(theBoard->mGridCelLook)); }); break;
			case 4U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mGridCelOffset, sizeof(theBoard->mGridCelOffset)); }); break;
			case 5U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mGridCelFog, sizeof(theBoard->mGridCelFog)); }); break;
			case 6U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mEnableGraveStones); }); break;
			case 7U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mSpecialGraveStoneX); }); break;
			case 8U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mSpecialGraveStoneY); }); break;
			case 9U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncFloat(theBoard->mFogOffset); }); break;
			case 10U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mFogBlownCountDown); }); break;
			case 11U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mPlantRow, sizeof(theBoard->mPlantRow)); }); break;
			case 12U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mWaveRowGotLawnMowered, sizeof(theBoard->mWaveRowGotLawnMowered)); }); break;
			case 13U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mBonusLawnMowersRemaining); }); break;
			case 14U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mIceMinX, sizeof(theBoard->mIceMinX)); }); break;
			case 15U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mIceTimer, sizeof(theBoard->mIceTimer)); }); break;
			case 16U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mIceParticleID, sizeof(theBoard->mIceParticleID)); }); break;
			case 17U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mRowPickingArray, sizeof(theBoard->mRowPickingArray)); }); break;
			case 18U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mZombiesInWave, sizeof(theBoard->mZombiesInWave)); }); break;
			case 19U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mZombieAllowed, sizeof(theBoard->mZombieAllowed)); }); break;
			case 20U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mSunCountDown); }); break;
			case 21U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mNumSunsFallen); }); break;
			case 22U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mShakeCounter); }); break;
			case 23U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mShakeAmountX); }); break;
			case 24U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mShakeAmountY); }); break;
			case 25U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mBackground, sizeof(theBoard->mBackground)); }); break;
			case 26U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mLevel); }); break;
			case 27U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mSodPosition); }); break;
			case 28U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mPrevMouseX); }); break;
			case 29U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mPrevMouseY); }); break;
			case 30U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mSunMoney); }); break;
			case 31U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mNumWaves); }); break;
			case 32U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mMainCounter); }); break;
			case 33U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mEffectCounter); }); break;
			case 34U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mDrawCount); }); break;
			case 35U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mRiseFromGraveCounter); }); break;
			case 36U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mOutOfMoneyCounter); }); break;
			case 37U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mCurrentWave); }); break;
			case 38U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mTotalSpawnedWaves); }); break;
			case 39U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mTutorialState, sizeof(theBoard->mTutorialState)); }); break;
			case 40U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mTutorialParticleID, sizeof(theBoard->mTutorialParticleID)); }); break;
			case 41U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mTutorialTimer); }); break;
			case 42U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mLastBungeeWave); }); break;
			case 43U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mZombieHealthToNextWave); }); break;
			case 44U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mZombieHealthWaveStart); }); break;
			case 45U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mZombieCountDown); }); break;
			case 46U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mZombieCountDownStart); }); break;
			case 47U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mHugeWaveCountDown); }); break;
			case 48U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mHelpDisplayed, sizeof(theBoard->mHelpDisplayed)); }); break;
			case 49U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mHelpIndex, sizeof(theBoard->mHelpIndex)); }); break;
			case 50U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mFinalBossKilled); }); break;
			case 51U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mShowShovel); }); break;
			case 52U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mCoinBankFadeCount); }); break;
			case 53U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mDebugTextMode, sizeof(theBoard->mDebugTextMode)); }); break;
			case 54U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mLevelComplete); }); break;
			case 55U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mBoardFadeOutCounter); }); break;
			case 56U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mNextSurvivalStageCounter); }); break;
			case 57U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mScoreNextMowerCounter); }); break;
			case 58U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mLevelAwardSpawned); }); break;
			case 59U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mProgressMeterWidth); }); break;
			case 60U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mFlagRaiseCounter); }); break;
			case 61U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mIceTrapCounter); }); break;
			case 62U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mBoardRandSeed); }); break;
			case 63U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mPoolSparklyParticleID, sizeof(theBoard->mPoolSparklyParticleID)); }); break;
			case 64U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mFwooshID, sizeof(theBoard->mFwooshID)); }); break;
			case 65U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mFwooshCountDown); }); break;
			case 66U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mTimeStopCounter); }); break;
			case 67U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mDroppedFirstCoin); }); break;
			case 68U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mFinalWaveSoundCounter); }); break;
			case 69U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mCobCannonCursorDelayCounter); }); break;
			case 70U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mCobCannonMouseX); }); break;
			case 71U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mCobCannonMouseY); }); break;
			case 72U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mKilledYeti); }); break;
			case 73U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mMustacheMode); }); break;
			case 74U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mSuperMowerMode); }); break;
			case 75U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mFutureMode); }); break;
			case 76U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mPinataMode); }); break;
			case 77U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mDanceMode); }); break;
			case 78U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mDaisyMode); }); break;
			case 79U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mSukhbirMode); }); break;
			case 80U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mPrevBoardResult, sizeof(theBoard->mPrevBoardResult)); }); break;
			case 81U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mTriggeredLawnMowers); }); break;
			case 82U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mPlayTimeActiveLevel); }); break;
			case 83U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mPlayTimeInactiveLevel); }); break;
			case 84U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mMaxSunPlants); }); break;
			case 85U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt64(theBoard->mStartDrawTime); }); break;
			case 86U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt64(theBoard->mIntervalDrawTime); }); break;
			case 87U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mIntervalDrawCountStart); }); break;
			case 88U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncFloat(theBoard->mMinFPS); }); break;
			case 89U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mPreloadTime); }); break;
			case 90U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ int64_t aGameId = (int64_t)theBoard->mGameID; c.SyncInt64(aGameId); if (c.mReading) theBoard->mGameID = (intptr_t)aGameId; }); break;
			case 91U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mGravesCleared); }); break;
			case 92U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mPlantsEaten); }); break;
			case 93U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mPlantsShoveled); }); break;
			case 94U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mPeaShooterUsed); }); break;
			case 95U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mCatapultPlantsUsed); }); break;
			case 96U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mMushroomAndCoffeeBeansOnly); }); break;
			case 97U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mMushroomsUsed); }); break;
			case 98U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mLevelCoinsCollected); }); break;
			case 99U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mGargantuarsKillsByCornCob); }); break;
			case 100U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mCoinsCollected); }); break;
			case 101U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mDiamondsCollected); }); break;
			case 102U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mPottedPlantsCollected); }); break;
			case 103U: ApplyFieldWithSync(aFieldData, aFieldSize, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mChocolateCollected); }); break;
			default: break;
			}
		}
	}
	else
	{
		std::vector<unsigned char> aBlob;
		AppendFieldWithSync(aBlob, 1U, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mPaused); });
		AppendFieldWithSync(aBlob, 2U, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mGridSquareType, sizeof(theBoard->mGridSquareType)); });
		AppendFieldWithSync(aBlob, 3U, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mGridCelLook, sizeof(theBoard->mGridCelLook)); });
		AppendFieldWithSync(aBlob, 4U, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mGridCelOffset, sizeof(theBoard->mGridCelOffset)); });
		AppendFieldWithSync(aBlob, 5U, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mGridCelFog, sizeof(theBoard->mGridCelFog)); });
		AppendFieldWithSync(aBlob, 6U, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mEnableGraveStones); });
		AppendFieldWithSync(aBlob, 7U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mSpecialGraveStoneX); });
		AppendFieldWithSync(aBlob, 8U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mSpecialGraveStoneY); });
		AppendFieldWithSync(aBlob, 9U, [&](PortableSaveContext& c){ c.SyncFloat(theBoard->mFogOffset); });
		AppendFieldWithSync(aBlob, 10U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mFogBlownCountDown); });
		AppendFieldWithSync(aBlob, 11U, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mPlantRow, sizeof(theBoard->mPlantRow)); });
		AppendFieldWithSync(aBlob, 12U, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mWaveRowGotLawnMowered, sizeof(theBoard->mWaveRowGotLawnMowered)); });
		AppendFieldWithSync(aBlob, 13U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mBonusLawnMowersRemaining); });
		AppendFieldWithSync(aBlob, 14U, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mIceMinX, sizeof(theBoard->mIceMinX)); });
		AppendFieldWithSync(aBlob, 15U, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mIceTimer, sizeof(theBoard->mIceTimer)); });
		AppendFieldWithSync(aBlob, 16U, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mIceParticleID, sizeof(theBoard->mIceParticleID)); });
		AppendFieldWithSync(aBlob, 17U, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mRowPickingArray, sizeof(theBoard->mRowPickingArray)); });
		AppendFieldWithSync(aBlob, 18U, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mZombiesInWave, sizeof(theBoard->mZombiesInWave)); });
		AppendFieldWithSync(aBlob, 19U, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mZombieAllowed, sizeof(theBoard->mZombieAllowed)); });
		AppendFieldWithSync(aBlob, 20U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mSunCountDown); });
		AppendFieldWithSync(aBlob, 21U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mNumSunsFallen); });
		AppendFieldWithSync(aBlob, 22U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mShakeCounter); });
		AppendFieldWithSync(aBlob, 23U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mShakeAmountX); });
		AppendFieldWithSync(aBlob, 24U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mShakeAmountY); });
		AppendFieldWithSync(aBlob, 25U, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mBackground, sizeof(theBoard->mBackground)); });
		AppendFieldWithSync(aBlob, 26U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mLevel); });
		AppendFieldWithSync(aBlob, 27U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mSodPosition); });
		AppendFieldWithSync(aBlob, 28U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mPrevMouseX); });
		AppendFieldWithSync(aBlob, 29U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mPrevMouseY); });
		AppendFieldWithSync(aBlob, 30U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mSunMoney); });
		AppendFieldWithSync(aBlob, 31U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mNumWaves); });
		AppendFieldWithSync(aBlob, 32U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mMainCounter); });
		AppendFieldWithSync(aBlob, 33U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mEffectCounter); });
		AppendFieldWithSync(aBlob, 34U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mDrawCount); });
		AppendFieldWithSync(aBlob, 35U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mRiseFromGraveCounter); });
		AppendFieldWithSync(aBlob, 36U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mOutOfMoneyCounter); });
		AppendFieldWithSync(aBlob, 37U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mCurrentWave); });
		AppendFieldWithSync(aBlob, 38U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mTotalSpawnedWaves); });
		AppendFieldWithSync(aBlob, 39U, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mTutorialState, sizeof(theBoard->mTutorialState)); });
		AppendFieldWithSync(aBlob, 40U, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mTutorialParticleID, sizeof(theBoard->mTutorialParticleID)); });
		AppendFieldWithSync(aBlob, 41U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mTutorialTimer); });
		AppendFieldWithSync(aBlob, 42U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mLastBungeeWave); });
		AppendFieldWithSync(aBlob, 43U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mZombieHealthToNextWave); });
		AppendFieldWithSync(aBlob, 44U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mZombieHealthWaveStart); });
		AppendFieldWithSync(aBlob, 45U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mZombieCountDown); });
		AppendFieldWithSync(aBlob, 46U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mZombieCountDownStart); });
		AppendFieldWithSync(aBlob, 47U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mHugeWaveCountDown); });
		AppendFieldWithSync(aBlob, 48U, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mHelpDisplayed, sizeof(theBoard->mHelpDisplayed)); });
		AppendFieldWithSync(aBlob, 49U, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mHelpIndex, sizeof(theBoard->mHelpIndex)); });
		AppendFieldWithSync(aBlob, 50U, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mFinalBossKilled); });
		AppendFieldWithSync(aBlob, 51U, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mShowShovel); });
		AppendFieldWithSync(aBlob, 52U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mCoinBankFadeCount); });
		AppendFieldWithSync(aBlob, 53U, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mDebugTextMode, sizeof(theBoard->mDebugTextMode)); });
		AppendFieldWithSync(aBlob, 54U, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mLevelComplete); });
		AppendFieldWithSync(aBlob, 55U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mBoardFadeOutCounter); });
		AppendFieldWithSync(aBlob, 56U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mNextSurvivalStageCounter); });
		AppendFieldWithSync(aBlob, 57U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mScoreNextMowerCounter); });
		AppendFieldWithSync(aBlob, 58U, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mLevelAwardSpawned); });
		AppendFieldWithSync(aBlob, 59U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mProgressMeterWidth); });
		AppendFieldWithSync(aBlob, 60U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mFlagRaiseCounter); });
		AppendFieldWithSync(aBlob, 61U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mIceTrapCounter); });
		AppendFieldWithSync(aBlob, 62U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mBoardRandSeed); });
		AppendFieldWithSync(aBlob, 63U, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mPoolSparklyParticleID, sizeof(theBoard->mPoolSparklyParticleID)); });
		AppendFieldWithSync(aBlob, 64U, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mFwooshID, sizeof(theBoard->mFwooshID)); });
		AppendFieldWithSync(aBlob, 65U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mFwooshCountDown); });
		AppendFieldWithSync(aBlob, 66U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mTimeStopCounter); });
		AppendFieldWithSync(aBlob, 67U, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mDroppedFirstCoin); });
		AppendFieldWithSync(aBlob, 68U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mFinalWaveSoundCounter); });
		AppendFieldWithSync(aBlob, 69U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mCobCannonCursorDelayCounter); });
		AppendFieldWithSync(aBlob, 70U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mCobCannonMouseX); });
		AppendFieldWithSync(aBlob, 71U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mCobCannonMouseY); });
		AppendFieldWithSync(aBlob, 72U, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mKilledYeti); });
		AppendFieldWithSync(aBlob, 73U, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mMustacheMode); });
		AppendFieldWithSync(aBlob, 74U, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mSuperMowerMode); });
		AppendFieldWithSync(aBlob, 75U, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mFutureMode); });
		AppendFieldWithSync(aBlob, 76U, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mPinataMode); });
		AppendFieldWithSync(aBlob, 77U, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mDanceMode); });
		AppendFieldWithSync(aBlob, 78U, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mDaisyMode); });
		AppendFieldWithSync(aBlob, 79U, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mSukhbirMode); });
		AppendFieldWithSync(aBlob, 80U, [&](PortableSaveContext& c){ c.SyncBytes(&theBoard->mPrevBoardResult, sizeof(theBoard->mPrevBoardResult)); });
		AppendFieldWithSync(aBlob, 81U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mTriggeredLawnMowers); });
		AppendFieldWithSync(aBlob, 82U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mPlayTimeActiveLevel); });
		AppendFieldWithSync(aBlob, 83U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mPlayTimeInactiveLevel); });
		AppendFieldWithSync(aBlob, 84U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mMaxSunPlants); });
		AppendFieldWithSync(aBlob, 85U, [&](PortableSaveContext& c){ c.SyncInt64(theBoard->mStartDrawTime); });
		AppendFieldWithSync(aBlob, 86U, [&](PortableSaveContext& c){ c.SyncInt64(theBoard->mIntervalDrawTime); });
		AppendFieldWithSync(aBlob, 87U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mIntervalDrawCountStart); });
		AppendFieldWithSync(aBlob, 88U, [&](PortableSaveContext& c){ c.SyncFloat(theBoard->mMinFPS); });
		AppendFieldWithSync(aBlob, 89U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mPreloadTime); });
		AppendFieldWithSync(aBlob, 90U, [&](PortableSaveContext& c){ int64_t aGameId = (int64_t)theBoard->mGameID; c.SyncInt64(aGameId); if (c.mReading) theBoard->mGameID = (intptr_t)aGameId; });
		AppendFieldWithSync(aBlob, 91U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mGravesCleared); });
		AppendFieldWithSync(aBlob, 92U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mPlantsEaten); });
		AppendFieldWithSync(aBlob, 93U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mPlantsShoveled); });
		AppendFieldWithSync(aBlob, 94U, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mPeaShooterUsed); });
		AppendFieldWithSync(aBlob, 95U, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mCatapultPlantsUsed); });
		AppendFieldWithSync(aBlob, 96U, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mMushroomAndCoffeeBeansOnly); });
		AppendFieldWithSync(aBlob, 97U, [&](PortableSaveContext& c){ c.SyncBool(theBoard->mMushroomsUsed); });
		AppendFieldWithSync(aBlob, 98U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mLevelCoinsCollected); });
		AppendFieldWithSync(aBlob, 99U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mGargantuarsKillsByCornCob); });
		AppendFieldWithSync(aBlob, 100U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mCoinsCollected); });
		AppendFieldWithSync(aBlob, 101U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mDiamondsCollected); });
		AppendFieldWithSync(aBlob, 102U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mPottedPlantsCollected); });
		AppendFieldWithSync(aBlob, 103U, [&](PortableSaveContext& c){ c.SyncInt32(theBoard->mChocolateCollected); });
		WriteTLVBlob(theContext, aBlob);
	}
}

static void SyncZombiesPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayPortableTLV(theContext, theBoard->mZombies,
		[&](std::vector<unsigned char>& aOut, Zombie& theZombie)
		{
			WriteGameObjectField(aOut, 1U, theZombie);
			WritePodTailField(aOut, 2U, theZombie, &Zombie::mZombieType);
		},
		[&](uint32_t aFieldId, const unsigned char* aData, size_t aSize, Zombie& theZombie)
		{
			switch (aFieldId)
			{
			case 1U: ReadGameObjectField(aData, aSize, theZombie); break;
			case 2U: ReadPodTailField(aData, aSize, theZombie, &Zombie::mZombieType); break;
			default: break;
			}
		});
}

static void SyncPlantsPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayPortableTLV(theContext, theBoard->mPlants,
		[&](std::vector<unsigned char>& aOut, Plant& thePlant)
		{
			WriteGameObjectField(aOut, 1U, thePlant);
			WritePodTailField(aOut, 2U, thePlant, &Plant::mSeedType);
			AppendFieldWithSync(aOut, 3U, [&](PortableSaveContext& c){ c.SyncEnum(thePlant.mSeedType); });
			AppendFieldWithSync(aOut, 4U, [&](PortableSaveContext& c){ c.SyncEnum(thePlant.mImitaterType); });
			AppendFieldWithSync(aOut, 5U, [&](PortableSaveContext& c){ c.SyncInt32(thePlant.mPottedPlantIndex); });
		},
		[&](uint32_t aFieldId, const unsigned char* aData, size_t aSize, Plant& thePlant)
		{
			switch (aFieldId)
			{
			case 1U: ReadGameObjectField(aData, aSize, thePlant); break;
			case 2U: ReadPodTailField(aData, aSize, thePlant, &Plant::mSeedType); break;
			case 3U: ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncEnum(thePlant.mSeedType); }); break;
			case 4U: ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncEnum(thePlant.mImitaterType); }); break;
			case 5U: ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& c){ c.SyncInt32(thePlant.mPottedPlantIndex); }); break;
			default: break;
			}
		});
}

static void SyncProjectilesPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayPortableTLV(theContext, theBoard->mProjectiles,
		[&](std::vector<unsigned char>& aOut, Projectile& theProjectile)
		{
			WriteGameObjectField(aOut, 1U, theProjectile);
			WritePodTailField(aOut, 2U, theProjectile, &Projectile::mMotionType);
			WritePodTailField(aOut, 3U, theProjectile, &Projectile::mFrame);
		},
		[&](uint32_t aFieldId, const unsigned char* aData, size_t aSize, Projectile& theProjectile)
		{
			switch (aFieldId)
			{
			case 1U: ReadGameObjectField(aData, aSize, theProjectile); break;
			case 2U: ReadPodTailField(aData, aSize, theProjectile, &Projectile::mMotionType); break;
			case 3U: ReadPodTailField(aData, aSize, theProjectile, &Projectile::mFrame); break;
			default: break;
			}
		});
}

static void SyncCoinsPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayPortableTLV(theContext, theBoard->mCoins,
		[&](std::vector<unsigned char>& aOut, Coin& theCoin)
		{
			WriteGameObjectField(aOut, 1U, theCoin);
			WritePodTailField(aOut, 2U, theCoin, &Coin::mType);
			WritePodTailField(aOut, 3U, theCoin, &Coin::mPosX);
		},
		[&](uint32_t aFieldId, const unsigned char* aData, size_t aSize, Coin& theCoin)
		{
			switch (aFieldId)
			{
			case 1U: ReadGameObjectField(aData, aSize, theCoin); break;
			case 2U: ReadPodTailField(aData, aSize, theCoin, &Coin::mType); break;
			case 3U: ReadPodTailField(aData, aSize, theCoin, &Coin::mPosX); break;
			default: break;
			}
		});
}

static void SyncMowersPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayPortableTLV(theContext, theBoard->mLawnMowers,
		[&](std::vector<unsigned char>& aOut, LawnMower& theMower)
		{
			WritePodTailField(aOut, 1U, theMower, &LawnMower::mPosX);
		},
		[&](uint32_t aFieldId, const unsigned char* aData, size_t aSize, LawnMower& theMower)
		{
			switch (aFieldId)
			{
			case 1U: ReadPodTailField(aData, aSize, theMower, &LawnMower::mPosX); break;
			default: break;
			}
		});
}

static void SyncGridItemsPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayPortableTLV(theContext, theBoard->mGridItems,
		[&](std::vector<unsigned char>& aOut, GridItem& theItem)
		{
			WritePodTailField(aOut, 1U, theItem, &GridItem::mGridItemType);
		},
		[&](uint32_t aFieldId, const unsigned char* aData, size_t aSize, GridItem& theItem)
		{
			switch (aFieldId)
			{
			case 1U: ReadPodTailField(aData, aSize, theItem, &GridItem::mGridItemType); break;
			default: break;
			}
		});
}

static void SyncParticleEmittersPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayIdsOnlyPortable(theContext, theBoard->mApp->mEffectSystem->mParticleHolder->mEmitters);
}

static void SyncParticlesPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayIdsOnlyPortable(theContext, theBoard->mApp->mEffectSystem->mParticleHolder->mParticles);
}

static void SyncParticleSystemsPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayPortableTLV(theContext, theBoard->mApp->mEffectSystem->mParticleHolder->mParticleSystems,
		[theBoard](std::vector<unsigned char>& aOut, TodParticleSystem& theSystem)
		{
			AppendFieldWithSync(aOut, 1U, [&](PortableSaveContext& aContext)
			{
				SyncParticleSystemPortable(theBoard, &theSystem, aContext);
			});
		},
		[theBoard](uint32_t aFieldId, const unsigned char* aData, size_t aSize, TodParticleSystem& theSystem)
		{
			if (aFieldId == 1U)
			{
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& aContext)
				{
					SyncParticleSystemPortable(theBoard, &theSystem, aContext);
				});
			}
		});
}

static void SyncReanimationsPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayPortableTLV(theContext, theBoard->mApp->mEffectSystem->mReanimationHolder->mReanimations,
		[theBoard](std::vector<unsigned char>& aOut, Reanimation& theReanimation)
		{
			AppendFieldWithSync(aOut, 1U, [&](PortableSaveContext& aContext)
			{
				SyncReanimationPortable(theBoard, &theReanimation, aContext);
			});
		},
		[theBoard](uint32_t aFieldId, const unsigned char* aData, size_t aSize, Reanimation& theReanimation)
		{
			if (aFieldId == 1U)
			{
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& aContext)
				{
					SyncReanimationPortable(theBoard, &theReanimation, aContext);
				});
			}
		});
}

static void SyncTrailsPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayPortableTLV(theContext, theBoard->mApp->mEffectSystem->mTrailHolder->mTrails,
		[theBoard](std::vector<unsigned char>& aOut, Trail& theTrail)
		{
			AppendFieldWithSync(aOut, 1U, [&](PortableSaveContext& aContext)
			{
				SyncTrailPortable(theBoard, &theTrail, aContext);
			});
		},
		[theBoard](uint32_t aFieldId, const unsigned char* aData, size_t aSize, Trail& theTrail)
		{
			if (aFieldId == 1U)
			{
				ApplyFieldWithSync(aData, aSize, [&](PortableSaveContext& aContext)
				{
					SyncTrailPortable(theBoard, &theTrail, aContext);
				});
			}
		});
}

static void SyncAttachmentsPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncDataArrayPortableTLV(theContext, theBoard->mApp->mEffectSystem->mAttachmentHolder->mAttachments,
		[&](std::vector<unsigned char>& aOut, Attachment& theAttachment)
		{
			WritePodTailField(aOut, 1U, theAttachment, &Attachment::mEffectArray);
		},
		[&](uint32_t aFieldId, const unsigned char* aData, size_t aSize, Attachment& theAttachment)
		{
			switch (aFieldId)
			{
			case 1U: ReadPodTailField(aData, aSize, theAttachment, &Attachment::mEffectArray); break;
			default: break;
			}
		});
}

static void SyncCursorPortable(PortableSaveContext& theContext, Board* theBoard)
{
	if (theContext.mReading)
	{
		std::vector<unsigned char> aBlob;
		if (!ReadTLVBlob(theContext, aBlob))
			return;
		TLVReader aReader(aBlob.data(), aBlob.size());
		while (aReader.mOk && aReader.mPos < aReader.mSize)
		{
			uint32_t aFieldId = 0;
			uint32_t aFieldSize = 0;
			if (!aReader.ReadU32(aFieldId) || !aReader.ReadU32(aFieldSize))
				break;
			const unsigned char* aFieldData = nullptr;
			if (!aReader.ReadBytes(aFieldData, aFieldSize))
				break;
			switch (aFieldId)
			{
			case 1U: ReadGameObjectField(aFieldData, aFieldSize, *theBoard->mCursorObject); break;
			case 2U: ReadPodTailField(aFieldData, aFieldSize, *theBoard->mCursorObject, &CursorObject::mSeedBankIndex); break;
			default: break;
			}
		}
	}
	else
	{
		std::vector<unsigned char> aBlob;
		WriteGameObjectField(aBlob, 1U, *theBoard->mCursorObject);
		WritePodTailField(aBlob, 2U, *theBoard->mCursorObject, &CursorObject::mSeedBankIndex);
		WriteTLVBlob(theContext, aBlob);
	}
}

static void SyncCursorPreviewPortable(PortableSaveContext& theContext, Board* theBoard)
{
	if (theContext.mReading)
	{
		std::vector<unsigned char> aBlob;
		if (!ReadTLVBlob(theContext, aBlob))
			return;
		TLVReader aReader(aBlob.data(), aBlob.size());
		while (aReader.mOk && aReader.mPos < aReader.mSize)
		{
			uint32_t aFieldId = 0;
			uint32_t aFieldSize = 0;
			if (!aReader.ReadU32(aFieldId) || !aReader.ReadU32(aFieldSize))
				break;
			const unsigned char* aFieldData = nullptr;
			if (!aReader.ReadBytes(aFieldData, aFieldSize))
				break;
			switch (aFieldId)
			{
			case 1U: ReadGameObjectField(aFieldData, aFieldSize, *theBoard->mCursorPreview); break;
			case 2U: ReadPodTailField(aFieldData, aFieldSize, *theBoard->mCursorPreview, &CursorPreview::mGridX); break;
			default: break;
			}
		}
	}
	else
	{
		std::vector<unsigned char> aBlob;
		WriteGameObjectField(aBlob, 1U, *theBoard->mCursorPreview);
		WritePodTailField(aBlob, 2U, *theBoard->mCursorPreview, &CursorPreview::mGridX);
		WriteTLVBlob(theContext, aBlob);
	}
}

static void SyncAdvicePortable(PortableSaveContext& theContext, Board* theBoard)
{
	if (theContext.mReading)
	{
		std::vector<unsigned char> aBlob;
		if (!ReadTLVBlob(theContext, aBlob))
			return;
		TLVReader aReader(aBlob.data(), aBlob.size());
		while (aReader.mOk && aReader.mPos < aReader.mSize)
		{
			uint32_t aFieldId = 0;
			uint32_t aFieldSize = 0;
			if (!aReader.ReadU32(aFieldId) || !aReader.ReadU32(aFieldSize))
				break;
			const unsigned char* aFieldData = nullptr;
			if (!aReader.ReadBytes(aFieldData, aFieldSize))
				break;
			switch (aFieldId)
			{
			case 1U: ReadPodTailField(aFieldData, aFieldSize, *theBoard->mAdvice, &MessageWidget::mLabel); break;
			default: break;
			}
		}
	}
	else
	{
		std::vector<unsigned char> aBlob;
		WritePodTailField(aBlob, 1U, *theBoard->mAdvice, &MessageWidget::mLabel);
		WriteTLVBlob(theContext, aBlob);
	}
}

static void SyncSeedBankPortable(PortableSaveContext& theContext, Board* theBoard)
{
	if (theContext.mReading)
	{
		std::vector<unsigned char> aBlob;
		if (!ReadTLVBlob(theContext, aBlob))
			return;
		TLVReader aReader(aBlob.data(), aBlob.size());
		while (aReader.mOk && aReader.mPos < aReader.mSize)
		{
			uint32_t aFieldId = 0;
			uint32_t aFieldSize = 0;
			if (!aReader.ReadU32(aFieldId) || !aReader.ReadU32(aFieldSize))
				break;
			const unsigned char* aFieldData = nullptr;
			if (!aReader.ReadBytes(aFieldData, aFieldSize))
				break;
			switch (aFieldId)
			{
			case 1U: ReadGameObjectField(aFieldData, aFieldSize, *theBoard->mSeedBank); break;
			case 2U: ReadPodTailField(aFieldData, aFieldSize, *theBoard->mSeedBank, &SeedBank::mNumPackets); break;
			case 3U: ReadPodTailField(aFieldData, aFieldSize, *theBoard->mSeedBank, &SeedBank::mCutSceneDarken); break;
			case 4U: ReadPodTailField(aFieldData, aFieldSize, *theBoard->mSeedBank, &SeedBank::mConveyorBeltCounter); break;
			default: break;
			}
		}
	}
	else
	{
		std::vector<unsigned char> aBlob;
		WriteGameObjectField(aBlob, 1U, *theBoard->mSeedBank);
		WritePodTailField(aBlob, 2U, *theBoard->mSeedBank, &SeedBank::mNumPackets);
		WritePodTailField(aBlob, 3U, *theBoard->mSeedBank, &SeedBank::mCutSceneDarken);
		WritePodTailField(aBlob, 4U, *theBoard->mSeedBank, &SeedBank::mConveyorBeltCounter);
		WriteTLVBlob(theContext, aBlob);
	}
}

static void SyncSeedPacketsPortable(PortableSaveContext& theContext, Board* theBoard)
{
	int aCount = SEEDBANK_MAX;
	theContext.SyncInt32(aCount);
	for (int i = 0; i < aCount && i < SEEDBANK_MAX; i++)
	{
		if (theContext.mReading)
		{
			uint32_t aItemSize = 0;
			theContext.SyncUInt32(aItemSize);
			std::vector<unsigned char> aItemData;
			aItemData.resize(aItemSize);
			if (aItemSize > 0)
				theContext.SyncBytes(aItemData.data(), aItemSize);
			TLVReader aReader(aItemData.data(), aItemSize);
			while (aReader.mOk && aReader.mPos < aReader.mSize)
			{
				uint32_t aFieldId = 0;
				uint32_t aFieldSize = 0;
				if (!aReader.ReadU32(aFieldId) || !aReader.ReadU32(aFieldSize))
					break;
				const unsigned char* aFieldData = nullptr;
				if (!aReader.ReadBytes(aFieldData, aFieldSize))
					break;
				switch (aFieldId)
				{
				case 1U: ReadGameObjectField(aFieldData, aFieldSize, theBoard->mSeedBank->mSeedPackets[i]); break;
				case 2U: ReadPodTailField(aFieldData, aFieldSize, theBoard->mSeedBank->mSeedPackets[i], &SeedPacket::mRefreshCounter); break;
				default: break;
				}
			}
		}
		else
		{
			std::vector<unsigned char> aItemData;
			WriteGameObjectField(aItemData, 1U, theBoard->mSeedBank->mSeedPackets[i]);
			WritePodTailField(aItemData, 2U, theBoard->mSeedBank->mSeedPackets[i], &SeedPacket::mRefreshCounter);
			uint32_t aItemSize = (uint32_t)aItemData.size();
			theContext.SyncUInt32(aItemSize);
			if (aItemSize > 0)
				theContext.SyncBytes(aItemData.data(), aItemSize);
		}
	}
}

static void SyncChallengePortable(PortableSaveContext& theContext, Board* theBoard)
{
	if (theContext.mReading)
	{
		std::vector<unsigned char> aBlob;
		if (!ReadTLVBlob(theContext, aBlob))
			return;
		TLVReader aReader(aBlob.data(), aBlob.size());
		while (aReader.mOk && aReader.mPos < aReader.mSize)
		{
			uint32_t aFieldId = 0;
			uint32_t aFieldSize = 0;
			if (!aReader.ReadU32(aFieldId) || !aReader.ReadU32(aFieldSize))
				break;
			const unsigned char* aFieldData = nullptr;
			if (!aReader.ReadBytes(aFieldData, aFieldSize))
				break;
			switch (aFieldId)
			{
			case 1U: ReadPodTailField(aFieldData, aFieldSize, *theBoard->mChallenge, &Challenge::mBeghouledMouseCapture); break;
			default: break;
			}
		}
	}
	else
	{
		std::vector<unsigned char> aBlob;
		WritePodTailField(aBlob, 1U, *theBoard->mChallenge, &Challenge::mBeghouledMouseCapture);
		WriteTLVBlob(theContext, aBlob);
	}
}

static void SyncMusicPortable(PortableSaveContext& theContext, Board* theBoard)
{
	if (theContext.mReading)
	{
		std::vector<unsigned char> aBlob;
		if (!ReadTLVBlob(theContext, aBlob))
			return;
		TLVReader aReader(aBlob.data(), aBlob.size());
		while (aReader.mOk && aReader.mPos < aReader.mSize)
		{
			uint32_t aFieldId = 0;
			uint32_t aFieldSize = 0;
			if (!aReader.ReadU32(aFieldId) || !aReader.ReadU32(aFieldSize))
				break;
			const unsigned char* aFieldData = nullptr;
			if (!aReader.ReadBytes(aFieldData, aFieldSize))
				break;
			switch (aFieldId)
			{
			case 1U: ReadPodTailField(aFieldData, aFieldSize, *theBoard->mApp->mMusic, &Music::mCurMusicTune); break;
			default: break;
			}
		}
	}
	else
	{
		std::vector<unsigned char> aBlob;
		WritePodTailField(aBlob, 1U, *theBoard->mApp->mMusic, &Music::mCurMusicTune);
		WriteTLVBlob(theContext, aBlob);
	}
}

static void SyncBoardPortable(PortableSaveContext& theContext, Board* theBoard)
{
	SyncBoardBasePortable(theContext, theBoard);
	SyncZombiesPortable(theContext, theBoard);
	SyncPlantsPortable(theContext, theBoard);
	SyncProjectilesPortable(theContext, theBoard);
	SyncCoinsPortable(theContext, theBoard);
	SyncMowersPortable(theContext, theBoard);
	SyncGridItemsPortable(theContext, theBoard);
	SyncParticleEmittersPortable(theContext, theBoard);
	SyncParticlesPortable(theContext, theBoard);
	SyncParticleSystemsPortable(theContext, theBoard);
	SyncReanimationsPortable(theContext, theBoard);
	SyncTrailsPortable(theContext, theBoard);
	SyncAttachmentsPortable(theContext, theBoard);
	SyncCursorPortable(theContext, theBoard);
	SyncCursorPreviewPortable(theContext, theBoard);
	SyncAdvicePortable(theContext, theBoard);
	SyncSeedBankPortable(theContext, theBoard);
	SyncSeedPacketsPortable(theContext, theBoard);
	SyncChallengePortable(theContext, theBoard);
	SyncMusicPortable(theContext, theBoard);
}


typedef void (*ChunkSyncFn)(PortableSaveContext&, Board*);

static ChunkSyncFn GetChunkSyncFn(uint32_t theChunkType)
{
	switch (theChunkType)
	{
	case SAVE4_CHUNK_BOARD_BASE:
		return SyncBoardBasePortable;
	case SAVE4_CHUNK_ZOMBIES:
		return SyncZombiesPortable;
	case SAVE4_CHUNK_PLANTS:
		return SyncPlantsPortable;
	case SAVE4_CHUNK_PROJECTILES:
		return SyncProjectilesPortable;
	case SAVE4_CHUNK_COINS:
		return SyncCoinsPortable;
	case SAVE4_CHUNK_MOWERS:
		return SyncMowersPortable;
	case SAVE4_CHUNK_GRIDITEMS:
		return SyncGridItemsPortable;
	case SAVE4_CHUNK_PARTICLE_EMITTERS:
		return SyncParticleEmittersPortable;
	case SAVE4_CHUNK_PARTICLE_PARTICLES:
		return SyncParticlesPortable;
	case SAVE4_CHUNK_PARTICLE_SYSTEMS:
		return SyncParticleSystemsPortable;
	case SAVE4_CHUNK_REANIMATIONS:
		return SyncReanimationsPortable;
	case SAVE4_CHUNK_TRAILS:
		return SyncTrailsPortable;
	case SAVE4_CHUNK_ATTACHMENTS:
		return SyncAttachmentsPortable;
	case SAVE4_CHUNK_CURSOR:
		return SyncCursorPortable;
	case SAVE4_CHUNK_CURSOR_PREVIEW:
		return SyncCursorPreviewPortable;
	case SAVE4_CHUNK_ADVICE:
		return SyncAdvicePortable;
	case SAVE4_CHUNK_SEEDBANK:
		return SyncSeedBankPortable;
	case SAVE4_CHUNK_SEEDPACKETS:
		return SyncSeedPacketsPortable;
	case SAVE4_CHUNK_CHALLENGE:
		return SyncChallengePortable;
	case SAVE4_CHUNK_MUSIC:
		return SyncMusicPortable;
	default:
		return nullptr;
	}
}

static bool WriteChunkV4(std::vector<unsigned char>& thePayload, uint32_t theChunkType, Board* theBoard)
{
	ChunkSyncFn aSyncFn = GetChunkSyncFn(theChunkType);
	if (!aSyncFn)
		return true;

	DataWriter aFieldWriter;
	aFieldWriter.OpenMemory(0x4000);
	PortableSaveContext aFieldContext(aFieldWriter);
	aSyncFn(aFieldContext, theBoard);
	if (aFieldContext.mFailed)
		return false;

	DataWriter aChunkWriter;
	aChunkWriter.OpenMemory(0x200);
	aChunkWriter.WriteLong(SAVE4_CHUNK_VERSION);
	aChunkWriter.WriteLong(1U);
	aChunkWriter.WriteLong((uint32_t)aFieldWriter.GetDataLen());
	aChunkWriter.WriteBytes(aFieldWriter.GetDataPtr(), aFieldWriter.GetDataLen());

	std::vector<unsigned char> aChunk;
	aChunk.resize(aChunkWriter.GetDataLen());
	memcpy(aChunk.data(), aChunkWriter.GetDataPtr(), aChunkWriter.GetDataLen());
	AppendChunk(thePayload, theChunkType, aChunk);
	return true;
}

static bool ReadChunkV4(uint32_t theChunkType, const unsigned char* theData, size_t theSize, Board* theBoard)
{
	ChunkSyncFn aSyncFn = GetChunkSyncFn(theChunkType);
	if (!aSyncFn)
		return true;
	if (theSize < 4)
		return false;

	TLVReader aReader(theData, theSize);
	uint32_t aChunkVersion = 0;
	if (!aReader.ReadU32(aChunkVersion))
		return false;
	if (aChunkVersion != SAVE4_CHUNK_VERSION)
		return false;

	bool aApplied = false;
	while (aReader.mOk && aReader.mPos < aReader.mSize)
	{
		uint32_t aFieldId = 0;
		uint32_t aFieldSize = 0;
		if (!aReader.ReadU32(aFieldId) || !aReader.ReadU32(aFieldSize))
			break;
		const unsigned char* aFieldData = nullptr;
		if (!aReader.ReadBytes(aFieldData, aFieldSize))
			break;

		if (aFieldId == 1U)
		{
			DataReader aFieldReader;
			aFieldReader.OpenMemory(aFieldData, (uint32_t)aFieldSize, false);
			PortableSaveContext aContext(aFieldReader);
			aSyncFn(aContext, theBoard);
			if (aContext.mFailed)
				return false;
			aApplied = true;
		}
	}

	return aApplied;
}

static bool LawnLoadGameV4(Board* theBoard, const std::string& theFilePath)
{
	Buffer aBuffer;
	if (!gSexyAppBase->ReadBufferFromFile(theFilePath, &aBuffer, false))
		return false;
	if ((unsigned int)aBuffer.GetDataLen() < sizeof(SaveFileHeaderV4))
		return false;

	SaveFileHeaderV4 aHeader;
	memcpy(&aHeader, aBuffer.GetDataPtr(), sizeof(aHeader));
	if (memcmp(aHeader.mMagic, SAVE_FILE_MAGIC_V4, sizeof(aHeader.mMagic)) != 0)
		return false;
	if (aHeader.mVersion != SAVE_FILE_V4_VERSION)
		return false;
	if (aHeader.mPayloadSize + sizeof(SaveFileHeaderV4) > (unsigned int)aBuffer.GetDataLen())
		return false;

	unsigned char* aPayload = (unsigned char*)aBuffer.GetDataPtr() + sizeof(SaveFileHeaderV4);
	unsigned int aCrc = crc32(0, (Bytef*)aPayload, aHeader.mPayloadSize);
	if (aCrc != aHeader.mPayloadCrc)
		return false;

	TLVReader aReader(aPayload, aHeader.mPayloadSize);
	bool aBaseLoaded = false;
	while (aReader.mOk && aReader.mPos < aReader.mSize)
	{
		uint32_t aChunkType = 0;
		uint32_t aChunkSize = 0;
		if (!aReader.ReadU32(aChunkType) || !aReader.ReadU32(aChunkSize))
			break;
		const unsigned char* aChunkData = nullptr;
		if (!aReader.ReadBytes(aChunkData, aChunkSize))
			break;

		if (!ReadChunkV4(aChunkType, aChunkData, aChunkSize, theBoard))
			return false;
		if (aChunkType == SAVE4_CHUNK_BOARD_BASE)
			aBaseLoaded = true;
	}

	if (!aBaseLoaded)
		return false;

	FixBoardAfterLoad(theBoard);
	theBoard->mApp->mGameScene = GameScenes::SCENE_PLAYING;
	return true;
}

//0x4813D0
void SaveGameContext::SyncBytes(void* theDest, int theReadSize)
{
	int aReadSize = theReadSize;
	if (mReading)
	{
		if ((unsigned long)ByteLeftToRead() < 4)
		{
			mFailed = true;
		}

		aReadSize = mFailed ? 0 : mBuffer.ReadLong();
	}
	else
	{
		mBuffer.WriteLong(theReadSize);
	}

	if (mReading)
	{
		if (aReadSize != theReadSize || ByteLeftToRead() < theReadSize)
		{
			mFailed = true;
		}

		if (mFailed)
		{
			memset(theDest, 0, theReadSize);
		}
		else
		{
			mBuffer.ReadBytes((uchar*)theDest, theReadSize);
		}
	}
	else
	{
		mBuffer.WriteBytes((uchar*)theDest, theReadSize);
	}
}

//0x481470
void SaveGameContext::SyncInt(int& theInt)
{
	if (mReading)
	{
		if ((unsigned long)ByteLeftToRead() < 4)
		{
			mFailed = true;
		}

		theInt = mFailed ? 0 : mBuffer.ReadLong();
	}
	else
	{
		mBuffer.WriteLong(theInt);
	}
}

//0x4814C0
void SaveGameContext::SyncReanimationDef(ReanimatorDefinition*& theDefinition)
{
	if (mReading)
	{
		int aReanimType;
		SyncInt(aReanimType);
		if (aReanimType == (int)ReanimationType::REANIM_NONE)
		{
			theDefinition = nullptr;
		}
		else if (aReanimType >= 0 && aReanimType < (int)ReanimationType::NUM_REANIMS)
		{
			ReanimatorEnsureDefinitionLoaded((ReanimationType)aReanimType, true);
			theDefinition = &gReanimatorDefArray[aReanimType];
		}
		else
		{
			mFailed = true;
		}
	}
	else
	{
		int aReanimType = (int)ReanimationType::REANIM_NONE;
		for (int i = 0; i < (int)ReanimationType::NUM_REANIMS; i++)
		{
			ReanimatorDefinition* aDef = &gReanimatorDefArray[i];
			if (theDefinition == aDef)
			{
				aReanimType = i;
				break;
			}
		}
		SyncInt(aReanimType);
	}
}

//0x481560
void SaveGameContext::SyncParticleDef(TodParticleDefinition*& theDefinition)
{
	if (mReading)
	{
		int aParticleType;
		SyncInt(aParticleType);
		if (aParticleType == (int)ParticleEffect::PARTICLE_NONE)
		{
			theDefinition = nullptr;
		}
		else if (aParticleType >= 0 && aParticleType < (int)ParticleEffect::NUM_PARTICLES)
		{
			theDefinition = &gParticleDefArray[aParticleType];
		}
		else
		{
			mFailed = true;
		}
	}
	else
	{
		int aParticleType = (int)ParticleEffect::PARTICLE_NONE;
		for (int i = 0; i < (int)ParticleEffect::NUM_PARTICLES; i++)
		{
			TodParticleDefinition* aDef = &gParticleDefArray[i];
			if (theDefinition == aDef)
			{
				aParticleType = i;
				break;
			}
		}
		SyncInt(aParticleType);
	}
}

//0x4815F0
void SaveGameContext::SyncTrailDef(TrailDefinition*& theDefinition)
{
	if (mReading)
	{
		int aTrailType;
		SyncInt(aTrailType);
		if (aTrailType == TrailType::TRAIL_NONE)
		{
			theDefinition = nullptr;
		}
		else if (aTrailType >= 0 && aTrailType < TrailType::NUM_TRAILS)
		{
			theDefinition = &gTrailDefArray[aTrailType];
		}
		else
		{
			mFailed = true;
		}
	}
	else
	{
		int aTrailType = TrailType::TRAIL_NONE;
		for (int i = 0; i < TrailType::NUM_TRAILS; i++)
		{
			TrailDefinition* aDef = &gTrailDefArray[i];
			if (theDefinition == aDef)
			{
				aTrailType = i;
				break;
			}
		}
		SyncInt(aTrailType);
	}
}

//0x481690
void SaveGameContext::SyncImage(Image*& theImage)
{
	if (mReading)
	{
		ResourceId aResID;
		SyncInt((int&)aResID);
		if (aResID == Sexy::ResourceId::RESOURCE_ID_MAX)
		{
			theImage = nullptr;
		}
		else
		{
			theImage = GetImageById(aResID);
		}
	}
	else
	{
		ResourceId aResID;
		if (theImage != nullptr)
		{
			aResID = GetIdByImage(theImage);
		}
		else
		{
			aResID = Sexy::ResourceId::RESOURCE_ID_MAX;
		}
		SyncInt((int&)aResID);
	}
}

//0x481710
void SyncDataIDList(TodList<unsigned int>* theDataIDList, SaveGameContext& theContext, TodAllocator* theAllocator)
{
	try
	{
		if (theContext.mReading)
		{
			if (theDataIDList)
			{
				theDataIDList->mHead = nullptr;
				theDataIDList->mTail = nullptr;
				theDataIDList->mSize = 0;
				theDataIDList->SetAllocator(theAllocator);
			}

			int aCount;
			theContext.SyncInt(aCount);
			for (int i = 0; i < aCount; i++)
			{
				unsigned int aDataID;
				theContext.SyncBytes(&aDataID, sizeof(aDataID));
				theDataIDList->AddTail(aDataID);
			}
		}
		else
		{
			int aCount = theDataIDList->mSize;
			theContext.SyncInt(aCount);
			for (TodListNode<unsigned int>* aNode = theDataIDList->mHead; aNode != nullptr; aNode = aNode->mNext)
			{
				unsigned int aDataID = aNode->mValue;
				theContext.SyncBytes(&aDataID, sizeof(aDataID));
			}
		}
	}
	catch (std::exception&)
	{
		return;
	}
}

//0x4817C0
void SyncParticleEmitter(TodParticleSystem* theParticleSystem, TodParticleEmitter* theParticleEmitter, SaveGameContext& theContext)
{
	int aEmitterDefIndex = 0;
	if (theContext.mReading)
	{
		theContext.SyncInt(aEmitterDefIndex);
		theParticleEmitter->mParticleSystem = theParticleSystem;
		theParticleEmitter->mEmitterDef = &theParticleSystem->mParticleDef->mEmitterDefs[aEmitterDefIndex];
	}
	else
	{
		aEmitterDefIndex = ((intptr_t)theParticleEmitter->mEmitterDef - (intptr_t)theParticleSystem->mParticleDef->mEmitterDefs) / sizeof(TodEmitterDefinition);
		theContext.SyncInt(aEmitterDefIndex);
	}

	theContext.SyncImage(theParticleEmitter->mImageOverride);
	SyncDataIDList((TodList<unsigned int>*)&theParticleEmitter->mParticleList, theContext, &theParticleSystem->mParticleHolder->mParticleListNodeAllocator);
	for (TodListNode<ParticleID>* aNode = theParticleEmitter->mParticleList.mHead; aNode != nullptr; aNode = aNode->mNext)
	{
		TodParticle* aParticle = theParticleSystem->mParticleHolder->mParticles.DataArrayGet((unsigned int)aNode->mValue);
		if (theContext.mReading)
		{
			aParticle->mParticleEmitter = theParticleEmitter;
		}
	}
}

//0x481880
void SyncParticleSystem(Board* theBoard, TodParticleSystem* theParticleSystem, SaveGameContext& theContext)
{
	theContext.SyncParticleDef(theParticleSystem->mParticleDef);
	if (theContext.mReading)
	{
		theParticleSystem->mParticleHolder = theBoard->mApp->mEffectSystem->mParticleHolder;
	}

	SyncDataIDList((TodList<unsigned int>*)&theParticleSystem->mEmitterList, theContext, &theParticleSystem->mParticleHolder->mEmitterListNodeAllocator);
	for (TodListNode<ParticleEmitterID>* aNode = theParticleSystem->mEmitterList.mHead; aNode != nullptr; aNode = aNode->mNext)
	{
		TodParticleEmitter* aEmitter = theParticleSystem->mParticleHolder->mEmitters.DataArrayGet((unsigned int)aNode->mValue);
		SyncParticleEmitter(theParticleSystem, aEmitter, theContext);
	}
}

//0x4818F0
void SyncReanimation(Board* theBoard, Reanimation* theReanimation, SaveGameContext& theContext)
{
	theContext.SyncReanimationDef(theReanimation->mDefinition);
	if (theContext.mReading)
	{
		theReanimation->mReanimationHolder = theBoard->mApp->mEffectSystem->mReanimationHolder;
	}

	if (theReanimation->mDefinition->mTracks.count != 0)
	{
		int aSize = theReanimation->mDefinition->mTracks.count * sizeof(ReanimatorTrackInstance);
		if (theContext.mReading)
		{
			theReanimation->mTrackInstances = (ReanimatorTrackInstance*)FindGlobalAllocator(aSize)->Calloc(aSize);
		}
		theContext.SyncBytes(theReanimation->mTrackInstances, aSize);

		for (int aTrackIndex = 0; aTrackIndex < theReanimation->mDefinition->mTracks.count; aTrackIndex++)
		{
			ReanimatorTrackInstance& aTrackInstance = theReanimation->mTrackInstances[aTrackIndex];
			theContext.SyncImage(aTrackInstance.mImageOverride);

			if (theContext.mReading)
			{
				aTrackInstance.mBlendTransform.mText = "";
				TOD_ASSERT(aTrackInstance.mBlendTransform.mFont == nullptr);
				TOD_ASSERT(aTrackInstance.mBlendTransform.mImage == nullptr);
			}
			else
			{
				TOD_ASSERT(aTrackInstance.mBlendTransform.mText[0] == 0);
				TOD_ASSERT(aTrackInstance.mBlendTransform.mFont == nullptr);
				TOD_ASSERT(aTrackInstance.mBlendTransform.mImage == nullptr);
			}
		}
	}
}

void SyncTrail(Board* theBoard, Trail* theTrail, SaveGameContext& theContext)
{
	theContext.SyncTrailDef(theTrail->mDefinition);
	if (theContext.mReading)
	{
		theTrail->mTrailHolder = theBoard->mApp->mEffectSystem->mTrailHolder;
	}
}

template <typename T> inline static void SyncDataArray(SaveGameContext& theContext, DataArray<T>& theDataArray)
{
	theContext.SyncUint(theDataArray.mFreeListHead);
	theContext.SyncUint(theDataArray.mMaxUsedCount);
	theContext.SyncUint(theDataArray.mSize);
	theContext.SyncBytes(theDataArray.mBlock, theDataArray.mMaxUsedCount * sizeof(*theDataArray.mBlock));
}

//0x4819D0
void SyncBoard(SaveGameContext& theContext, Board* theBoard)
{
	// TODO test if gives sane results
	size_t offset = size_t(&theBoard->mPaused) - size_t(theBoard);
	theContext.SyncBytes(&theBoard->mPaused, sizeof(Board) - offset);

	SyncDataArray(theContext, theBoard->mZombies);													//0x482190
	SyncDataArray(theContext, theBoard->mPlants);													//0x482280
	SyncDataArray(theContext, theBoard->mProjectiles);												//0x482370
	SyncDataArray(theContext, theBoard->mCoins);													//0x482460
	SyncDataArray(theContext, theBoard->mLawnMowers);												//0x482550
	SyncDataArray(theContext, theBoard->mGridItems);												//0x482650
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mParticleHolder->mParticleSystems);	//0x482740
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mParticleHolder->mEmitters);			//0x482830
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mParticleHolder->mParticles);			//0x482920
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mReanimationHolder->mReanimations);	//0x482920
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mTrailHolder->mTrails);				//0x482650
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mAttachmentHolder->mAttachments);		//0x482A10

	{
		TodParticleSystem* aParticle = nullptr;
		while (theBoard->mApp->mEffectSystem->mParticleHolder->mParticleSystems.IterateNext(aParticle))
		{
			SyncParticleSystem(theBoard, aParticle, theContext);
		}
	}
	{
		Reanimation* aReanimation = nullptr;
		while (theBoard->mApp->mEffectSystem->mReanimationHolder->mReanimations.IterateNext(aReanimation))
		{
			SyncReanimation(theBoard, aReanimation, theContext);
		}
	}
	{
		Trail* aTrail = nullptr;
		while (theBoard->mApp->mEffectSystem->mTrailHolder->mTrails.IterateNext(aTrail))
		{
			SyncTrail(theBoard, aTrail, theContext);
		}
	}

	theContext.SyncBytes(theBoard->mCursorObject, sizeof(CursorObject));
	theContext.SyncBytes(theBoard->mCursorPreview, sizeof(CursorPreview));
	theContext.SyncBytes(theBoard->mAdvice, sizeof(MessageWidget));
	theContext.SyncBytes(theBoard->mSeedBank, sizeof(SeedBank));
	theContext.SyncBytes(theBoard->mChallenge, sizeof(Challenge));
	theContext.SyncBytes(theBoard->mApp->mMusic, sizeof(Music));
	
	if (theContext.mReading)
	{
		if ((unsigned long)theContext.ByteLeftToRead() < 4)
		{
			theContext.mFailed = true;
		}

		if (theContext.mFailed || (unsigned int)theContext.mBuffer.ReadLong() != SAVE_FILE_MAGIC_NUMBER)
		{
			theContext.mFailed = true;
		}
	}
	else
	{
		theContext.mBuffer.WriteLong(SAVE_FILE_MAGIC_NUMBER);
	}
}

//0x481CE0
void FixBoardAfterLoad(Board* theBoard)
{
	{
		Plant* aPlant = nullptr;
		while (theBoard->mPlants.IterateNext(aPlant))
		{
			aPlant->mApp = theBoard->mApp;
			aPlant->mBoard = theBoard;
		}
	}
	{
		Zombie* aZombie = nullptr;
		while (theBoard->mZombies.IterateNext(aZombie))
		{
			aZombie->mApp = theBoard->mApp;
			aZombie->mBoard = theBoard;

			switch (aZombie->mZombieType)
			{
			case ZombieType::ZOMBIE_GARGANTUAR:
			case ZombieType::ZOMBIE_REDEYE_GARGANTUAR:
			{
				Reanimation* aBodyReanim = theBoard->mApp->ReanimationGet(aZombie->mBodyReanimID);
				if (aBodyReanim)
				{
					int aDamageIndex = aZombie->GetBodyDamageIndex();
					if (aDamageIndex >= 1)
					{
						aBodyReanim->SetImageOverride("Zombie_gargantua_body1", IMAGE_REANIM_ZOMBIE_GARGANTUAR_BODY1_2);
						aBodyReanim->SetImageOverride("Zombie_gargantuar_outerarm_lower", IMAGE_REANIM_ZOMBIE_GARGANTUAR_OUTERARM_LOWER2);
					}
					if (aDamageIndex >= 2)
					{
						aBodyReanim->SetImageOverride("Zombie_gargantua_body1", IMAGE_REANIM_ZOMBIE_GARGANTUAR_BODY1_3);
						aBodyReanim->SetImageOverride("Zombie_gargantuar_outerleg_foot", IMAGE_REANIM_ZOMBIE_GARGANTUAR_FOOT2);
					}

					if (aZombie->mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
					{
						if (aDamageIndex >= 2)
							aBodyReanim->SetImageOverride("anim_head1", IMAGE_REANIM_ZOMBIE_GARGANTUAR_HEAD2_REDEYE);
						else
							aBodyReanim->SetImageOverride("anim_head1", IMAGE_REANIM_ZOMBIE_GARGANTUAR_HEAD_REDEYE);
					}
					else if (aDamageIndex >= 2)
					{
						aBodyReanim->SetImageOverride("anim_head1", IMAGE_REANIM_ZOMBIE_GARGANTUAR_HEAD2);
					}
				}
				break;
			}

			case ZombieType::ZOMBIE_ZAMBONI:
			{
				Reanimation* aBodyReanim = theBoard->mApp->ReanimationGet(aZombie->mBodyReanimID);
				if (aBodyReanim)
				{
					int aDamageIndex = aZombie->GetBodyDamageIndex();
					if (aDamageIndex >= 1)
					{
						aBodyReanim->SetImageOverride("Zombie_zamboni_1", IMAGE_REANIM_ZOMBIE_ZAMBONI_1_DAMAGE1);
						aBodyReanim->SetImageOverride("Zombie_zamboni_2", IMAGE_REANIM_ZOMBIE_ZAMBONI_2_DAMAGE1);
					}
					if (aDamageIndex >= 2)
					{
						aBodyReanim->SetImageOverride("Zombie_zamboni_1", IMAGE_REANIM_ZOMBIE_ZAMBONI_1_DAMAGE2);
						aBodyReanim->SetImageOverride("Zombie_zamboni_2", IMAGE_REANIM_ZOMBIE_ZAMBONI_2_DAMAGE2);
					}
				}
				break;
			}

			case ZombieType::ZOMBIE_CATAPULT:
			{
				Reanimation* aBodyReanim = theBoard->mApp->ReanimationGet(aZombie->mBodyReanimID);
				if (aBodyReanim)
				{
					int aDamageIndex = aZombie->GetBodyDamageIndex();
					if (aDamageIndex >= 1)
					{
						aBodyReanim->SetImageOverride("Zombie_catapult_siding", IMAGE_REANIM_ZOMBIE_CATAPULT_SIDING_DAMAGE);
					}
				}
				break;
			}

			case ZombieType::ZOMBIE_BOSS:
			{
				Reanimation* aBodyReanim = theBoard->mApp->ReanimationGet(aZombie->mBodyReanimID);
				if (aBodyReanim)
				{
					int aDamageIndex = aZombie->GetBodyDamageIndex();
					if (aDamageIndex >= 1)
					{
						aBodyReanim->SetImageOverride("Boss_head", IMAGE_REANIM_ZOMBIE_BOSS_HEAD_DAMAGE1);
						aBodyReanim->SetImageOverride("Boss_jaw", IMAGE_REANIM_ZOMBIE_BOSS_JAW_DAMAGE1);
						aBodyReanim->SetImageOverride("Boss_outerarm_hand", IMAGE_REANIM_ZOMBIE_BOSS_OUTERARM_HAND_DAMAGE1);
						aBodyReanim->SetImageOverride("Boss_outerarm_thumb2", IMAGE_REANIM_ZOMBIE_BOSS_OUTERARM_THUMB_DAMAGE1);
						aBodyReanim->SetImageOverride("Boss_innerleg_foot", IMAGE_REANIM_ZOMBIE_BOSS_FOOT_DAMAGE1);
					}
					if (aDamageIndex >= 2)
					{
						aBodyReanim->SetImageOverride("Boss_head", IMAGE_REANIM_ZOMBIE_BOSS_HEAD_DAMAGE2);
						aBodyReanim->SetImageOverride("Boss_jaw", IMAGE_REANIM_ZOMBIE_BOSS_JAW_DAMAGE2);
						aBodyReanim->SetImageOverride("Boss_outerarm_hand", IMAGE_REANIM_ZOMBIE_BOSS_OUTERARM_HAND_DAMAGE2);
						aBodyReanim->SetImageOverride("Boss_outerarm_thumb2", IMAGE_REANIM_ZOMBIE_BOSS_OUTERARM_THUMB_DAMAGE2);
						aBodyReanim->SetImageOverride("Boss_outerleg_foot", IMAGE_REANIM_ZOMBIE_BOSS_FOOT_DAMAGE2);
					}
				}
				break;
			}

			default:
				break;
			}
		}
	}
	{
		Projectile* aProjectile = nullptr;
		while (theBoard->mProjectiles.IterateNext(aProjectile))
		{
			aProjectile->mApp = theBoard->mApp;
			aProjectile->mBoard = theBoard;
		}
	}
	{
		Coin* aCoin = nullptr;
		while (theBoard->mCoins.IterateNext(aCoin))
		{
			aCoin->mApp = theBoard->mApp;
			aCoin->mBoard = theBoard;
		}
	}
	{
		LawnMower* aLawnMower = nullptr;
		while (theBoard->mLawnMowers.IterateNext(aLawnMower))
		{
			aLawnMower->mApp = theBoard->mApp;
			aLawnMower->mBoard = theBoard;
		}
	}
	{
		GridItem* aGridItem = nullptr;
		while (theBoard->mGridItems.IterateNext(aGridItem))
		{
			aGridItem->mApp = theBoard->mApp;
			aGridItem->mBoard = theBoard;
		}
	}

	theBoard->mAdvice->mApp = theBoard->mApp;
	theBoard->mCursorObject->mApp = theBoard->mApp;
	theBoard->mCursorObject->mBoard = theBoard;
	theBoard->mCursorPreview->mApp = theBoard->mApp;
	theBoard->mCursorPreview->mBoard = theBoard;
	theBoard->mSeedBank->mApp = theBoard->mApp;
	theBoard->mSeedBank->mBoard = theBoard;
	for (int i = 0; i < SEEDBANK_MAX; i++)
	{
		theBoard->mSeedBank->mSeedPackets[i].mApp = theBoard->mApp;
		theBoard->mSeedBank->mSeedPackets[i].mBoard = theBoard;
	}
	theBoard->mChallenge->mApp = theBoard->mApp;
	theBoard->mChallenge->mBoard = theBoard;
	theBoard->mApp->mMusic->mApp = theBoard->mApp;
	theBoard->mApp->mMusic->mMusicInterface = theBoard->mApp->mMusicInterface;
}

//0x481FE0
// GOTY @Patoke: 0x48CBC0
bool LawnLoadGame(Board* theBoard, const std::string& theFilePath)
{
	if (LawnLoadGameV4(theBoard, theFilePath))
	{
		TodTrace("Loaded save game (v4)");
		return true;
	}

	SaveGameContext aContext;
	aContext.mFailed = false;
	aContext.mReading = true;
	if (!gSexyAppBase->ReadBufferFromFile(theFilePath, &aContext.mBuffer, false))
	{
		return false;
	}

	SaveFileHeader aHeader;
	aContext.SyncBytes(&aHeader, sizeof(aHeader));
	if (aHeader.mMagicNumber != SAVE_FILE_MAGIC_NUMBER || aHeader.mBuildVersion != SAVE_FILE_VERSION || aHeader.mBuildDate != SAVE_FILE_DATE)
	{
		return false;
	}

	SyncBoard(aContext, theBoard);
	if (aContext.mFailed)
	{
		return false;
	}

	TodTrace("Loaded save game (legacy)");
	FixBoardAfterLoad(theBoard);
	theBoard->mApp->mGameScene = GameScenes::SCENE_PLAYING;
	return true;
}

//0x4820D0
bool LawnSaveGame(Board* theBoard, const std::string& theFilePath)
{
	std::vector<unsigned char> aPayload;
	if (!WriteChunkV4(aPayload, SAVE4_CHUNK_BOARD_BASE, theBoard)) return false;
	if (!WriteChunkV4(aPayload, SAVE4_CHUNK_ZOMBIES, theBoard)) return false;
	if (!WriteChunkV4(aPayload, SAVE4_CHUNK_PLANTS, theBoard)) return false;
	if (!WriteChunkV4(aPayload, SAVE4_CHUNK_PROJECTILES, theBoard)) return false;
	if (!WriteChunkV4(aPayload, SAVE4_CHUNK_COINS, theBoard)) return false;
	if (!WriteChunkV4(aPayload, SAVE4_CHUNK_MOWERS, theBoard)) return false;
	if (!WriteChunkV4(aPayload, SAVE4_CHUNK_GRIDITEMS, theBoard)) return false;
	if (!WriteChunkV4(aPayload, SAVE4_CHUNK_PARTICLE_EMITTERS, theBoard)) return false;
	if (!WriteChunkV4(aPayload, SAVE4_CHUNK_PARTICLE_PARTICLES, theBoard)) return false;
	if (!WriteChunkV4(aPayload, SAVE4_CHUNK_PARTICLE_SYSTEMS, theBoard)) return false;
	if (!WriteChunkV4(aPayload, SAVE4_CHUNK_REANIMATIONS, theBoard)) return false;
	if (!WriteChunkV4(aPayload, SAVE4_CHUNK_TRAILS, theBoard)) return false;
	if (!WriteChunkV4(aPayload, SAVE4_CHUNK_ATTACHMENTS, theBoard)) return false;
	if (!WriteChunkV4(aPayload, SAVE4_CHUNK_CURSOR, theBoard)) return false;
	if (!WriteChunkV4(aPayload, SAVE4_CHUNK_CURSOR_PREVIEW, theBoard)) return false;
	if (!WriteChunkV4(aPayload, SAVE4_CHUNK_ADVICE, theBoard)) return false;
	if (!WriteChunkV4(aPayload, SAVE4_CHUNK_SEEDBANK, theBoard)) return false;
	if (!WriteChunkV4(aPayload, SAVE4_CHUNK_SEEDPACKETS, theBoard)) return false;
	if (!WriteChunkV4(aPayload, SAVE4_CHUNK_CHALLENGE, theBoard)) return false;
	if (!WriteChunkV4(aPayload, SAVE4_CHUNK_MUSIC, theBoard)) return false;

	SaveFileHeaderV4 aHeader{};
	memcpy(aHeader.mMagic, SAVE_FILE_MAGIC_V4, sizeof(aHeader.mMagic));
	aHeader.mVersion = SAVE_FILE_V4_VERSION;
	aHeader.mPayloadSize = (unsigned int)aPayload.size();
	aHeader.mPayloadCrc = crc32(0, (Bytef*)aPayload.data(), (unsigned int)aPayload.size());

	std::vector<unsigned char> aOutBuffer;
	aOutBuffer.resize(sizeof(aHeader) + aPayload.size());
	memcpy(aOutBuffer.data(), &aHeader, sizeof(aHeader));
	memcpy(aOutBuffer.data() + sizeof(aHeader), aPayload.data(), aPayload.size());

	return gSexyAppBase->WriteBytesToFile(theFilePath, aOutBuffer.data(), (int)aOutBuffer.size());
}

bool LawnSaveGameLegacy(Board* theBoard, const std::string& theFilePath)
{
	SaveGameContext aContext;
	aContext.mFailed = false;
	aContext.mReading = false;

	SaveFileHeader aHeader;
	aHeader.mMagicNumber = SAVE_FILE_MAGIC_NUMBER;
	aHeader.mBuildVersion = SAVE_FILE_VERSION;
	aHeader.mBuildDate = SAVE_FILE_DATE;

	aContext.SyncBytes(&aHeader, sizeof(aHeader));
	SyncBoard(aContext, theBoard);
	return gSexyAppBase->WriteBufferToFile(theFilePath, &aContext.mBuffer);
}
