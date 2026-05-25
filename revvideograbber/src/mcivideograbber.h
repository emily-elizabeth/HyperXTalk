#ifndef WINVIDEO_H

#define WINVIDEO_H
#include "videograbber.h"
#include <Windows.h>
#include <Vfw.h>


class CWinVideoGrabber: public CVideoGrabber
{
public:
	  CWinVideoGrabber(HWND whichwindow);
	  CWinVideoGrabber();
	  virtual ~CWinVideoGrabber();
	  Bool IsInited() {return inited;}
	  void DoIdle();
	  void Init();
	  void SetRect(short left, short top, short right, short bottom);
	  void GetRect(short *left, short *top, short *right, short *bottom);
	  void SetVisible(Bool tvisible);
	  void StartPreviewing();
	  void StopPreviewing();
	  void StartRecording(char *filename);
	  void StopRecording();
	  void GetSettingsString(ExternalString &s);
	  void SetSettingsString(ExternalString &s);
	  void VideoFormatDialog();
	  void VideoSourceDialog();
	  void VideoDisplayDialog();
	  void VideoDefaultDialog();
	  void AudioDefaultDialog() {VideoDefaultDialog();}
	  void VideoCompressionDialog();
	 
	  void SetAudioCapture(Bool bSet);
	  void setCompressor(char* codecName);
	  char *getCompressors();
	  void AddError(char *serr);
	  void setAudioFormat(int channels, int bits, int frequency);
	  void getAudioFormat(int* channels, int* bits, int* frequency);
	  Bool GetAudioCapture(void);
	  void setAudioCompressor(char *codecName){};

	  //get all available codec names, return a '\n' delimited list.
	  //the caller must call  FreeMemory() to free the string buffer
	  char *getAudioCompressors(){return NULL;}
	  char *GetCurrentCodecName(){return NULL;}
	  char *GetCurrentAudioCodecName(){return NULL;}
	  void SetFrameRate(int framerate);
	  void GetFrameRate(double *frate);
	  void GetFrameSize(int *fwidth,int *fheight);
	  void SetFrameSize(int fwidth,int fheight);

#ifdef WIN32
	  Bool Draw(int twidth,int theight, HDC hdcMem);
#endif
	 
protected:
	  Bool inited;
	  HWND parentwindow;
	  HWND videowindow;
	  Bool buffervideo;
	  RECT destvideorect;
	  RECT srcvideorect;
	  static int winid;
};

#endif
