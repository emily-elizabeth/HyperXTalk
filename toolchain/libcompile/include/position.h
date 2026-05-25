#ifndef __POSITION__
#define __POSITION__

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef intptr_t PositionRef;
typedef struct File *FileRef;
    
void InitializePosition(void);
void FinalizePosition(void);

void AdvanceCurrentPosition(intptr_t delta);
void AdvanceCurrentPositionToNextRow(void);
void AdvanceCurrentPositionToFile(FileRef file);

void GetColumnOfPosition(PositionRef position, intptr_t *r_column);
void GetRowOfPosition(PositionRef position, intptr_t *r_row);
void GetFileOfPosition(PositionRef position, FileRef *r_file);
void GetFilenameOfPosition(PositionRef position, const char **r_filename);
void GetRowTextOfPosition(PositionRef position, const char **r_text);

void GetCurrentPosition(PositionRef *r_result);
void yyGetPos(PositionRef *r_result);

void InitializeFiles(void);
void FinalizeFiles(void);
    
void AddFile(const char *filename);
int MoveToNextFile(void);
void GetFilePath(FileRef file, const char **r_path);
void GetFileName(FileRef file, const char **r_name);
void GetFileIndex(FileRef file, intptr_t *r_index);
const char *GetFileLineText(FileRef file, intptr_t p_row);
int GetFileWithIndex(intptr_t index, FileRef *r_file);
int GetCurrentFile(FileRef *r_file);

#ifdef __cplusplus
}
#endif

#endif
