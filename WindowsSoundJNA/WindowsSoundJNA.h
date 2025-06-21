// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the WINDOWSSOUNDJNA_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see
// WINDOWSSOUNDJNA_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef WINDOWSSOUNDJNA_EXPORTS
#define WINDOWSSOUNDJNA_API __declspec(dllexport)
#else
#define WINDOWSSOUNDJNA_API __declspec(dllimport)
#endif

// This class is exported from the dll
/*class WINDOWSSOUNDJNA_API CWindowsSoundJNA {
public:
	CWindowsSoundJNA(void);
	// TODO: add your methods here.
};

extern WINDOWSSOUNDJNA_API int nWindowsSoundJNA;

WINDOWSSOUNDJNA_API int fnWindowsSoundJNA(void);*/

typedef int(__stdcall *wmmCallback)(const char* data, int dataLen);

extern "C" {
	
	WINDOWSSOUNDJNA_API int enumerateDevices();

	WINDOWSSOUNDJNA_API int getNumDevices();

	WINDOWSSOUNDJNA_API char* getDeviceName(int iDevice);

	WINDOWSSOUNDJNA_API WCHAR* getDeviceName2(int iDevice);

	WINDOWSSOUNDJNA_API int getDeviceFormats(int iDevice);

	WINDOWSSOUNDJNA_API int getDeviceChannels(int iDevice);

	WINDOWSSOUNDJNA_API int wavePrepare(int iDevice, int nChannels, int sampleRate, int bitDepth, wmmCallback callBackFn);

	WINDOWSSOUNDJNA_API int waveStart();
	
	WINDOWSSOUNDJNA_API int waveStop();






	// functions using the WASAPI instead of the older mm functions
	// see https://learn.microsoft.com/en-us/windows/win32/multimedia/waveform-audio

	//WINDOWSSOUNDJNA_API int wEnumerateDevices();


}
