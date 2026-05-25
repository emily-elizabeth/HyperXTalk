extern void REVVideoGrabber(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);
extern void REVVideo_Version(char *args[], int nargs, char **retstring,
		   Bool *pass, Bool *error);
extern void revInitializeVideoGrabber(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);
extern void revPreviewVideo(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);
extern void revStopPreviewingVideo(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);
extern void revRecordVideo(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);
extern void revStopRecordingVideo(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);
extern void revVideoGrabIdle(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);
extern void revVideoGrabDialog(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);
extern void revVideoGrabSettings(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);
extern void revSetVideoGrabSettings(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);
extern void revSetVideoGrabberRect(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);
extern void revVideoFrameImage(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);
extern void revCloseVideoGrabber(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);
extern void  revSetVideoGrabFrameRate(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);
extern void  revGetVideoGrabAudio(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);
extern void  revSetVideoGrabAudio(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);
extern void  revSetVideoGrabCompressor(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);


extern void  revSetVideoColorSpace(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);

extern void revVideoGrabCompressors(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);
extern void  revSetVideoGrabFrameSize(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);
extern void  revGetVideoGrabFrameSize(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);
extern void  revGetVideoGrabFrameRate(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);
extern void revVideoGrabAudioCompressors(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);
extern void  revGetVideoGrabAudioCompressor(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);
extern void  revSetVideoGrabAudioCompressor(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);
extern void  revGetVideoGrabCompressor(char *args[], int nargs, char **retstring,
	       Bool *pass, Bool *error);


extern void VideoGrabberDoIdle();
void VIDEOGRABBER_INIT();
void VIDEOGRABBER_QUIT();




