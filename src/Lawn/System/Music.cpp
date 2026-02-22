#include "Music.h"
#include "../Board.h"
#include "PlayerInfo.h"
#include "../../LawnApp.h"
#include "paklib/PakInterface.h"
#include "../../Sexy.TodLib/TodDebug.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "sound/SDLMusicInterface.h"

using namespace Sexy;

//0x45A260
Music::Music()
{
	mApp = (LawnApp*)gSexyAppBase;
	mMusicInterface = gSexyAppBase->mMusicInterface;
	mCurMusicTune = MusicTune::MUSIC_TUNE_NONE;
	mCurMusicFileMain = MusicFile::MUSIC_FILE_NONE;
	mCurMusicFileDrums = MusicFile::MUSIC_FILE_NONE;
	mCurMusicFileHihats = MusicFile::MUSIC_FILE_NONE;
	mBurstOverride = -1;
	mMusicDrumsState = MusicDrumsState::MUSIC_DRUMS_OFF;
	mQueuedDrumTrackPackedOrder = -1;
	mBaseBPM = 155;
	mBaseModSpeed = 3;
	mMusicBurstState = MusicBurstState::MUSIC_BURST_OFF;
	mPauseOffset = 0;
	mPauseOffsetDrums = 0;
	mPaused = false;
	mMusicDisabled = false;
	mFadeOutCounter = 0;
	mFadeOutDuration = 0;
}

MusicFileData gMusicFileData[MusicFile::NUM_MUSIC_FILES];  //0x6A9ED0

//0x45A2C0
bool Music::TodLoadMusic(MusicFile theMusicFile, const std::string& theFileName)
{
	Mix_Music* aHMusic = 0;
	SDLMusicInterface* anSDL = (SDLMusicInterface*)mApp->mMusicInterface;
	std::string anExt;

	size_t aDot = theFileName.rfind('.');
	if (aDot != std::string::npos)
		anExt = StringToLower(theFileName.substr(aDot + 1));

	PFILE* pFile = p_fopen(theFileName.c_str(), "rb");
	if (pFile == nullptr)
		return false;

	p_fseek(pFile, 0, SEEK_END);
	int aSize = p_ftell(pFile);
	p_fseek(pFile, 0, SEEK_SET);
	void* aData = operator new[](aSize);
	p_fread(aData, sizeof(char), aSize, pFile);
	p_fclose(pFile);

	aHMusic = Mix_LoadMUS_RW(SDL_RWFromMem(aData, aSize), 1);
	if (theMusicFile == MusicFile::MUSIC_FILE_CREDITS_ZOMBIES_ON_YOUR_LAWN)
	{
		gMusicFileData[theMusicFile].mFileData = (unsigned int*)aData;
	}
	else
	{
		delete[] (char *)aData;
	}

	if (aHMusic == 0)
		return false;

	SDLMusicInfo aMusicInfo;
	aMusicInfo.mHMusic = aHMusic;
	anSDL->mMusicMap.insert(SDLMusicMap::value_type(theMusicFile, aMusicInfo));
	return true;
}

void Music::SetupDrumsVolumeForTune(MusicTune theMusicTune, float volume)
{
	const int aTrackCount = 30;
	int aTrackStart1 = -1, aTrackEnd1 = -1;

	switch (theMusicTune)
	{
	case MusicTune::MUSIC_TUNE_DAY_GRASSWALK:		aTrackStart1 = 0;	aTrackEnd1 = 23;	break;
	case MusicTune::MUSIC_TUNE_POOL_WATERYGRAVES:	aTrackStart1 = 0;	aTrackEnd1 = 17;	break;
	case MusicTune::MUSIC_TUNE_FOG_RIGORMORMIST:	aTrackStart1 = 0;	aTrackEnd1 = 15;	break;
	case MusicTune::MUSIC_TUNE_ROOF_GRAZETHEROOF:	aTrackStart1 = 0;	aTrackEnd1 = 17;	break;
	default:
		aTrackStart1 = 0;
		aTrackEnd1 = 29;
		break;
	}

	Mix_Music* aHMusic = GetMusicHandle(MusicFile::MUSIC_FILE_MAIN_MUSIC);
	for (int aTrack = 0; aTrack < aTrackCount; aTrack++)
	{
		float aVolume;
		if (aTrack >= aTrackStart1 && aTrack <= aTrackEnd1)
			aVolume = 1;
		else
			aVolume = volume;

		Mix_ModMusicStreamSetChannelVolume(aHMusic, aTrack, (int)(aVolume*128));
	}
}

void Music::LoadSong(MusicFile theMusicFile, const std::string& theFileName)
{
	TodHesitationTrace("preloadsong");
	if (!TodLoadMusic(theMusicFile, theFileName))
	{
		TodTrace("music failed to load\n");
		mMusicDisabled = true;
	}
	else
	{
		TodHesitationTrace("song '%s'", theFileName.c_str());
	}
}

//0x45A8A0
void Music::MusicTitleScreenInit()
{
	LoadSong(MusicFile::MUSIC_FILE_MAIN_MUSIC, "sounds/mainmusic.mo3");
	MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_TITLE_CRAZY_DAVE_MAIN_THEME);
}

//0x45A980
void Music::MusicInit()
{
#ifdef _PVZ_DEBUG
	int aNumLoadingTasks = mApp->mCompletedLoadingThreadTasks + GetNumLoadingTasks();
#endif

	LoadSong(MusicFile::MUSIC_FILE_DRUMS, "sounds/mainmusic.mo3");
	mApp->mCompletedLoadingThreadTasks += 3500;
	LoadSong(MusicFile::MUSIC_FILE_HIHATS, "sounds/mainmusic_hihats.mo3");
	mApp->mCompletedLoadingThreadTasks += 3500;

	LoadSong(MusicFile::MUSIC_FILE_CREDITS_ZOMBIES_ON_YOUR_LAWN, "sounds/ZombiesOnYourLawn.ogg");
	mApp->mCompletedLoadingThreadTasks += 3500;

#ifdef _PVZ_DEBUG
	if (mApp->mCompletedLoadingThreadTasks != aNumLoadingTasks)
		TodTrace("Didn't calculate loading task count correctly!!!!");
#endif
}

//0x45AAC0
void Music::MusicCreditScreenInit()
{
	SDLMusicInterface* anSDL = (SDLMusicInterface*)mApp->mMusicInterface;
	if (anSDL->mMusicMap.find((int)MusicFile::MUSIC_FILE_CREDITS_ZOMBIES_ON_YOUR_LAWN) == anSDL->mMusicMap.end())
		LoadSong(MusicFile::MUSIC_FILE_CREDITS_ZOMBIES_ON_YOUR_LAWN, "sounds/ZombiesOnYourLawn.ogg");
}

//0x45ABB0
void Music::StopAllMusic()
{
	if (mMusicInterface != nullptr)
	{
		if (mCurMusicFileMain != MusicFile::MUSIC_FILE_NONE)
			mMusicInterface->StopMusic(mCurMusicFileMain);
		if (mCurMusicFileDrums != MusicFile::MUSIC_FILE_NONE)
			mMusicInterface->StopMusic(mCurMusicFileDrums);
	}

	mCurMusicTune = MusicTune::MUSIC_TUNE_NONE;
	mCurMusicFileMain = MusicFile::MUSIC_FILE_NONE;
	mCurMusicFileDrums = MusicFile::MUSIC_FILE_NONE;
	mCurMusicFileHihats = MusicFile::MUSIC_FILE_NONE;
	mQueuedDrumTrackPackedOrder = -1;
	mMusicDrumsState = MusicDrumsState::MUSIC_DRUMS_OFF;
	mMusicBurstState = MusicBurstState::MUSIC_BURST_OFF;
	mPauseOffset = 0;
	mPauseOffsetDrums = 0;
	mPaused = false;
	mFadeOutCounter = 0;
}

//0x45AC20
Mix_Music* Music::GetMusicHandle(MusicFile theMusicFile)
{
	SDLMusicInterface* anSDL = (SDLMusicInterface*)mApp->mMusicInterface;
	auto anItr = anSDL->mMusicMap.find((int)theMusicFile);
	TOD_ASSERT(anItr != anSDL->mMusicMap.end());
	return anItr->second.mHMusic;
}

//0x45AC70
void Music::PlayFromOffset(MusicFile theMusicFile, int theOffset, double theVolume)
{
	SDLMusicInterface* anSDL = (SDLMusicInterface*)mApp->mMusicInterface;
	auto anItr = anSDL->mMusicMap.find((int)theMusicFile);
	TOD_ASSERT(anItr != anSDL->mMusicMap.end());
	SDLMusicInfo* aMusicInfo = &anItr->second;

	if (mCurMusicTune == MusicTune::MUSIC_TUNE_CREDITS_ZOMBIES_ON_YOUR_LAWN)
	{
		bool aNoLoop = theMusicFile == MusicFile::MUSIC_FILE_CREDITS_ZOMBIES_ON_YOUR_LAWN;
		mMusicInterface->PlayMusic(theMusicFile, theOffset, aNoLoop);
	}
	else
	{
		Mix_HaltMusicStream(aMusicInfo->mHMusic);
		aMusicInfo->mStopOnFade = false;
		aMusicInfo->mVolume = aMusicInfo->mVolumeCap * theVolume;
		aMusicInfo->mVolumeAdd = 0.0;
		Mix_PlayMusicStream(aMusicInfo->mHMusic, -1);
		Mix_ModMusicStreamJumpToOrder(aMusicInfo->mHMusic, theOffset);
		Mix_VolumeMusicStream(aMusicInfo->mHMusic, (int)(aMusicInfo->mVolume*128));
		SetupDrumsVolumeForTune(mCurMusicTune, 0);
	}
}

//0x45ADB0
void Music::PlayMusic(MusicTune theMusicTune, int theOffset, int theDrumsOffset)
{
	if (mMusicDisabled)
		return;

	mCurMusicTune = theMusicTune;
	mCurMusicFileMain = MusicFile::MUSIC_FILE_NONE;
	mCurMusicFileDrums = MusicFile::MUSIC_FILE_NONE;
	mCurMusicFileHihats = MusicFile::MUSIC_FILE_NONE;
	bool aRestartingSong = theOffset != -1;

	switch (theMusicTune)
	{
	case MusicTune::MUSIC_TUNE_DAY_GRASSWALK:
		mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
		mCurMusicFileDrums = MusicFile::MUSIC_FILE_DRUMS;
		if (theOffset == -1)
			theOffset = 0;
		PlayFromOffset(mCurMusicFileMain, theOffset, 1.0);
		break;

	case MusicTune::MUSIC_TUNE_NIGHT_MOONGRAINS:
		mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
		mCurMusicFileDrums = MusicFile::MUSIC_FILE_DRUMS;
		if (theOffset == -1)
		{
			theOffset = 0x30;
			theDrumsOffset = 0x5C;
		}
		PlayFromOffset(mCurMusicFileMain, theOffset, 1.0);
		PlayFromOffset(mCurMusicFileDrums, theDrumsOffset, 0.0);
		break;

	case MusicTune::MUSIC_TUNE_POOL_WATERYGRAVES:
		mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
		mCurMusicFileDrums = MusicFile::MUSIC_FILE_DRUMS;
		if (theOffset == -1)
			theOffset = 0x5E;
		PlayFromOffset(mCurMusicFileMain, theOffset, 1.0);
		break;

	case MusicTune::MUSIC_TUNE_FOG_RIGORMORMIST:
		mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
		mCurMusicFileDrums = MusicFile::MUSIC_FILE_DRUMS;
		if (theOffset == -1)
			theOffset = 0x7D;
		PlayFromOffset(mCurMusicFileMain, theOffset, 1.0);
		break;

	case MusicTune::MUSIC_TUNE_ROOF_GRAZETHEROOF:
		mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
		mCurMusicFileDrums = MusicFile::MUSIC_FILE_DRUMS;
		if (theOffset == -1)
			theOffset = 0xB8;
		PlayFromOffset(mCurMusicFileMain, theOffset, 1.0);
		break;

	case MusicTune::MUSIC_TUNE_CHOOSE_YOUR_SEEDS:
		mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
		if (theOffset == -1)
			theOffset = 0x7A;
		PlayFromOffset(mCurMusicFileMain, theOffset, 1.0);
		break;

	case MusicTune::MUSIC_TUNE_TITLE_CRAZY_DAVE_MAIN_THEME:
		mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
		if (theOffset == -1)
			theOffset = 0x98;
		PlayFromOffset(mCurMusicFileMain, theOffset, 1.0);
		break;

	case MusicTune::MUSIC_TUNE_ZEN_GARDEN:
		mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
		if (theOffset == -1)
			theOffset = 0xDD;
		PlayFromOffset(mCurMusicFileMain, theOffset, 1.0);
		break;

	case MusicTune::MUSIC_TUNE_PUZZLE_CEREBRAWL:
		mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
		if (theOffset == -1)
			theOffset = 0xB1;
		PlayFromOffset(mCurMusicFileMain, theOffset, 1.0);
		break;

	case MusicTune::MUSIC_TUNE_MINIGAME_LOONBOON:
		mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
		if (theOffset == -1)
			theOffset = 0xA6;
		PlayFromOffset(mCurMusicFileMain, theOffset, 1.0);
		break;

	case MusicTune::MUSIC_TUNE_CONVEYER:
		mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
		if (theOffset == -1)
			theOffset = 0xD4;
		PlayFromOffset(mCurMusicFileMain, theOffset, 1.0);
		break;

	case MusicTune::MUSIC_TUNE_FINAL_BOSS_BRAINIAC_MANIAC:
		mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
		if (theOffset == -1)
			theOffset = 0x9E;
		PlayFromOffset(mCurMusicFileMain, theOffset, 1.0);
		break;

	case MusicTune::MUSIC_TUNE_CREDITS_ZOMBIES_ON_YOUR_LAWN:
		mCurMusicFileMain = MusicFile::MUSIC_FILE_CREDITS_ZOMBIES_ON_YOUR_LAWN;
		if (theOffset == -1)
			theOffset = 0;
		PlayFromOffset(mCurMusicFileMain, theOffset, 1.0);
		break;

	default:
		TOD_ASSERT(false);
		break;
	}

	if (aRestartingSong)
	{
		// TODO: Restore BPM/speed for restarting songs when tempo API is implemented
	}
	else
	{
		// TODO: Read base BPM/speed from newly started song when tempo API is implemented
	}
}

unsigned long Music::GetMusicOrder(MusicFile theMusicFile)
{
	TOD_ASSERT(theMusicFile != MusicFile::MUSIC_FILE_NONE);
	return ((SDLMusicInterface*)mApp->mMusicInterface)->GetMusicOrder((int)theMusicFile);
}

//0x45B1B0
void Music::MusicResyncChannel(MusicFile theMusicFileToMatch, MusicFile theMusicFileToSync)
{
	unsigned int aPosToMatch = GetMusicOrder(theMusicFileToMatch);
	unsigned int aPosToSync = GetMusicOrder(theMusicFileToSync);
	int aDiff = (aPosToSync >> 16) - (aPosToMatch >> 16);  // Row diff between two channels
	if (abs(aDiff) <= 128)
	{
		int aBPM = mBaseBPM;
		if (aDiff > 2)
			aBPM -= 2;
		else if (aDiff > 0)
			aBPM -= 1;
		else if (aDiff < -2)
			aBPM += 2;
		else if (aDiff < 0)
			aBPM -= 1;

		// TODO: Apply BPM adjustment when tempo API is implemented
	}
}

void Music::MusicResync()
{
	if (mCurMusicFileMain != MusicFile::MUSIC_FILE_NONE)
	{
		if (mCurMusicFileDrums != MusicFile::MUSIC_FILE_NONE)
			MusicResyncChannel(mCurMusicFileMain, mCurMusicFileDrums);
	}
}

//0x45B240
void Music::StartBurst()
{ 
	// No burst in intro
	if (mApp->mGameMode == GameMode::GAMEMODE_INTRO)
		return;
	if (mMusicBurstState == MusicBurstState::MUSIC_BURST_OFF)
	{ 
		mMusicBurstState = MusicBurstState::MUSIC_BURST_STARTING;
		mBurstStateCounter = 400;
	}
}

void Music::FadeOut(int theFadeOutDuration)
{ 
	if (mCurMusicTune != MusicTune::MUSIC_TUNE_NONE)
	{
		mFadeOutCounter = theFadeOutDuration;
		mFadeOutDuration = theFadeOutDuration;
	}
}

//0x45B260
void Music::UpdateMusicBurst()
{
	if (mApp->mBoard == nullptr)
		return;

	int aBurstScheme;
	if (mCurMusicTune == MusicTune::MUSIC_TUNE_DAY_GRASSWALK || mCurMusicTune == MusicTune::MUSIC_TUNE_POOL_WATERYGRAVES ||
		mCurMusicTune == MusicTune::MUSIC_TUNE_FOG_RIGORMORMIST || mCurMusicTune == MusicTune::MUSIC_TUNE_ROOF_GRAZETHEROOF)
		aBurstScheme = 1;
	else if (mCurMusicTune == MusicTune::MUSIC_TUNE_NIGHT_MOONGRAINS)
		aBurstScheme = 2;
	else
		return;

	int aPackedOrderMain = GetMusicOrder(mCurMusicFileMain);
	if (mBurstStateCounter > 0)
		mBurstStateCounter--;
	if (mDrumsStateCounter > 0)
		mDrumsStateCounter--;

	float aFadeTrackVolume = 0.0f;
	float aDrumsVolume = 0.0f;
	float aMainTrackVolume = 1.0f;
	switch (mMusicBurstState)
	{
		case MusicBurstState::MUSIC_BURST_OFF:
			if (mApp->mBoard->CountZombiesOnScreen() >= 10 || mBurstOverride == 1)
				StartBurst();
			break;
		case MusicBurstState::MUSIC_BURST_STARTING:
			if (aBurstScheme == 1)
			{
				aFadeTrackVolume = TodAnimateCurveFloat(400, 0, mBurstStateCounter, 0.0f, 1.0f, TodCurves::CURVE_LINEAR);
				if (mBurstStateCounter == 200)
				{
					mMusicDrumsState = MusicDrumsState::MUSIC_DRUMS_ON_QUEUED;
					mQueuedDrumTrackPackedOrder = aPackedOrderMain;
				}
				else if (mBurstStateCounter == 0)
				{
					mMusicBurstState = MusicBurstState::MUSIC_BURST_ON;
					mBurstStateCounter = 800;
				}
			}
			else if (aBurstScheme == 2)
			{
				if (mMusicDrumsState == MusicDrumsState::MUSIC_DRUMS_OFF)
				{
					mMusicDrumsState = MusicDrumsState::MUSIC_DRUMS_ON_QUEUED;
					mQueuedDrumTrackPackedOrder = aPackedOrderMain;
					mBurstStateCounter = 400;
				}
				else if (mMusicDrumsState == MusicDrumsState::MUSIC_DRUMS_ON_QUEUED)
					mBurstStateCounter = 400;
				else
				{
					aMainTrackVolume = TodAnimateCurveFloat(400, 0, mBurstStateCounter, 1.0f, 0.0f, TodCurves::CURVE_LINEAR);
					if (mBurstStateCounter == 0)
					{
						mMusicBurstState = MusicBurstState::MUSIC_BURST_ON;
						mBurstStateCounter = 800;
					}
				}
			}
			break;
		case MusicBurstState::MUSIC_BURST_ON:
			aFadeTrackVolume = 1.0f;
			if (aBurstScheme == 2)
				aMainTrackVolume = 0.0f;
			if (mBurstStateCounter == 0 && ((mApp->mBoard->CountZombiesOnScreen() < 4 && mBurstOverride == -1) || mBurstOverride == 2))
			{
				if (aBurstScheme == 1)
				{
					mMusicBurstState = MusicBurstState::MUSIC_BURST_FINISHING;
					mBurstStateCounter = 800;
					mMusicDrumsState = MusicDrumsState::MUSIC_DRUMS_OFF_QUEUED;
					mQueuedDrumTrackPackedOrder = aPackedOrderMain;
				}
				else if (aBurstScheme == 2)
				{
					mMusicBurstState = MusicBurstState::MUSIC_BURST_FINISHING;
					mBurstStateCounter = 1100;
					mMusicDrumsState = MusicDrumsState::MUSIC_DRUMS_FADING;
					mDrumsStateCounter = 800;
				}
			}
			break;
		case MusicBurstState::MUSIC_BURST_FINISHING:
			if (aBurstScheme == 1)
				aFadeTrackVolume = TodAnimateCurveFloat(800, 0, mBurstStateCounter, 1.0f, 0.0f, TodCurves::CURVE_LINEAR);
			else
				aMainTrackVolume = TodAnimateCurveFloat(400, 0, mBurstStateCounter, 0.0f, 1.0f, TodCurves::CURVE_LINEAR);
			if (mBurstStateCounter == 0 && mMusicDrumsState == MusicDrumsState::MUSIC_DRUMS_OFF)
				mMusicBurstState = MusicBurstState::MUSIC_BURST_OFF;
			break;
	}

	int aDrumsJumpOrder = -1;
	int aOrderMain = 0, aOrderDrum = 0;
	if (aBurstScheme == 1)
	{
		aOrderMain = aPackedOrderMain & 0xFFFF;
		aOrderDrum = mQueuedDrumTrackPackedOrder & 0xFFFF;
	}
	else if (aBurstScheme == 2)
	{
		aOrderMain = aPackedOrderMain & 0xFFFF;
		aOrderDrum = mQueuedDrumTrackPackedOrder & 0xFFFF;
	}

	switch (mMusicDrumsState)
	{
		case MusicDrumsState::MUSIC_DRUMS_ON_QUEUED:
			if (aOrderMain != aOrderDrum)
			{
				aDrumsVolume = 1.0f;
				mMusicDrumsState = MusicDrumsState::MUSIC_DRUMS_ON;
				if (aBurstScheme == 2)
					aDrumsJumpOrder = (aOrderMain % 2 == 0) ? 76 : 77;
			}
			break;
		case MusicDrumsState::MUSIC_DRUMS_ON:
			aDrumsVolume = 1.0f;
			break;
		case MusicDrumsState::MUSIC_DRUMS_OFF_QUEUED:
			aDrumsVolume = 1.0f;
			if (aOrderMain != aOrderDrum && aBurstScheme == 1)
			{
				mMusicDrumsState = MusicDrumsState::MUSIC_DRUMS_FADING;
				mDrumsStateCounter = 50;
			}
			break;
		case MusicDrumsState::MUSIC_DRUMS_FADING:
			if (aBurstScheme == 2)
				aDrumsVolume = TodAnimateCurveFloat(800, 0, mDrumsStateCounter, 1.0f, 0.0f, TodCurves::CURVE_LINEAR);
			else
				aDrumsVolume = TodAnimateCurveFloat(50, 0, mDrumsStateCounter, 1.0f, 0.0f, TodCurves::CURVE_LINEAR);
			if (mDrumsStateCounter == 0)
				mMusicDrumsState = MusicDrumsState::MUSIC_DRUMS_OFF;
			break;
		case MusicDrumsState::MUSIC_DRUMS_OFF:
			break;
	}

	if (aBurstScheme == 1)
	{
		SetupDrumsVolumeForTune(mCurMusicTune, aDrumsVolume);
	}
	else if (aBurstScheme == 2)
	{
		mMusicInterface->SetSongVolume(mCurMusicFileMain, aMainTrackVolume);
		mMusicInterface->SetSongVolume(mCurMusicFileDrums, aDrumsVolume);
		if (aDrumsJumpOrder != -1)
			Mix_ModMusicStreamJumpToOrder(GetMusicHandle(mCurMusicFileDrums), aDrumsJumpOrder);
	}
}

//0x45B670
void Music::MusicUpdate()
{
	if (mFadeOutCounter > 0)
	{
		mFadeOutCounter--;
		if (mFadeOutCounter == 0)
			StopAllMusic();
		else
		{
			float aFadeLevel = TodAnimateCurveFloat(mFadeOutDuration, 0, mFadeOutCounter, 1.0f, 0.0f, TodCurves::CURVE_LINEAR);
			mMusicInterface->SetSongVolume(mCurMusicFileMain, aFadeLevel);
		}
	}

	if (mApp->mBoard == nullptr || !mApp->mBoard->mPaused)
	{
		UpdateMusicBurst();
		MusicResync();
	}
}

//0x45B750
// GOTY @Patoke: 0x45EFA0
void Music::MakeSureMusicIsPlaying(MusicTune theMusicTune)
{
	if (mCurMusicTune != theMusicTune)
	{
		StopAllMusic();
		PlayMusic(theMusicTune, -1, -1);
	}
}

//0x45B770
void Music::StartGameMusic()
{
	TOD_ASSERT(mApp->mBoard);

	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
		MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_ZEN_GARDEN);
	else if (mApp->IsFinalBossLevel())
		MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_FINAL_BOSS_BRAINIAC_MANIAC);
	else if (mApp->IsWallnutBowlingLevel() || mApp->IsWhackAZombieLevel() || mApp->IsLittleTroubleLevel() || mApp->IsBungeeBlitzLevel() ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_SPEED)
		MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_MINIGAME_LOONBOON);
	else if ((mApp->IsAdventureMode() && (mApp->mPlayerInfo->GetLevel() == 10 || mApp->mPlayerInfo->GetLevel() == 20 || mApp->mPlayerInfo->GetLevel() == 30)) ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_COLUMN)
		MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_CONVEYER);
	else if (mApp->IsStormyNightLevel())
		StopAllMusic();
	else if (mApp->IsScaryPotterLevel() || mApp->IsIZombieLevel())
		MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_PUZZLE_CEREBRAWL);
	else if (mApp->mBoard->StageHasFog())
		MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_FOG_RIGORMORMIST);
	else if (mApp->mBoard->StageIsNight())
		MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_NIGHT_MOONGRAINS);
	else if (mApp->mBoard->StageHas6Rows())
		MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_POOL_WATERYGRAVES);
	else if (mApp->mBoard->StageHasRoof())
		MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_ROOF_GRAZETHEROOF);
	else
		MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_DAY_GRASSWALK);
}

//0x45B930
void Music::GameMusicPause(bool thePause)
{
	if (thePause)
	{
		if (!mPaused && mCurMusicTune != MusicTune::MUSIC_TUNE_NONE)
		{
			// Keep offsets for save/load compatibility
			if (mCurMusicFileMain != MusicFile::MUSIC_FILE_NONE &&
				mCurMusicTune != MusicTune::MUSIC_TUNE_CREDITS_ZOMBIES_ON_YOUR_LAWN)
			{
				mPauseOffset = GetMusicOrder(mCurMusicFileMain);
			}
			if (mCurMusicTune == MusicTune::MUSIC_TUNE_NIGHT_MOONGRAINS &&
				mCurMusicFileDrums != MusicFile::MUSIC_FILE_NONE)
			{
				mPauseOffsetDrums = GetMusicOrder(mCurMusicFileDrums);
			}

			if (mCurMusicFileMain != MusicFile::MUSIC_FILE_NONE)
				mMusicInterface->PauseMusic(mCurMusicFileMain);
			if (mCurMusicFileDrums != MusicFile::MUSIC_FILE_NONE)
				mMusicInterface->PauseMusic(mCurMusicFileDrums);

			mPaused = true;
		}
	}
	else if (mPaused)
	{
		if (mCurMusicTune != MusicTune::MUSIC_TUNE_NONE)
		{
			Mix_Music* aHandle = (mCurMusicFileMain != MusicFile::MUSIC_FILE_NONE) ?
								  GetMusicHandle(mCurMusicFileMain) : nullptr;
			if (mCurMusicTune == MusicTune::MUSIC_TUNE_CREDITS_ZOMBIES_ON_YOUR_LAWN ||
				(aHandle && Mix_PlayingMusicStream(aHandle)))
			{
				if (mCurMusicFileMain != MusicFile::MUSIC_FILE_NONE)
					mMusicInterface->ResumeMusic(mCurMusicFileMain);
				if (mCurMusicFileDrums != MusicFile::MUSIC_FILE_NONE)
					mMusicInterface->ResumeMusic(mCurMusicFileDrums);
			}
			else
			{
				// Save-load resume path
				PlayMusic(mCurMusicTune, mPauseOffset, mPauseOffsetDrums);
			}
		}
		mPaused = false;
	}
}

int Music::GetNumLoadingTasks()
{
	return 3500 * 2;
}
