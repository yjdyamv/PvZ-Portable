#ifndef __SAVEGAMECONTEXT_H__
#define __SAVEGAMECONTEXT_H__

#include <string>

class Board;

bool				LawnLoadGame(Board* theBoard, const std::string& theFilePath);
bool				LawnSaveGame(Board* theBoard, const std::string& theFilePath);

#endif
