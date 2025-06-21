// WindowsSoundJNA.cpp : Defines the exported functions for the DLL.
//

#include "pch.h"
#include "framework.h"
#include "WindowsSoundJNA.h"
#include "stdio.h"
#include "stdlib.h"

#include "mmeapi.h"
#include "mmdeviceapi.h"

#pragma comment(lib, "WinMM.Lib")

// This is an example of an exported variable
//WINDOWSSOUNDJNA_API int nWindowsSoundJNA=0;

// This is an example of an exported function.
//WINDOWSSOUNDJNA_API int fnWindowsSoundJNA(void)
//{
//    return 0;
//}

// This is the constructor of a class that has been exported.
//CWindowsSoundJNA::CWindowsSoundJNA()
//{
//    return;
//}
BOOL WINAPI DllMain(
	HINSTANCE hinstDLL,  // handle to DLL module
	DWORD fdwReason,     // reason for calling function
	LPVOID lpReserved)  // reserved
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		//printf("In sound card DLL entry function reason %d\n", fdwReason);
		//wEnumerateDevices();
		fflush(stdout);
		break;
	}
	return TRUE;
}

int nDevices = 0;
WAVEINCAPS* deviceCaps = NULL;
int* usedDevices = NULL;

HWAVEIN hWaveIn;

#define NBUFFERS 10
int bufferLenBytes;
int currentBuffer = 0;
void** buffers = NULL;
static LPWAVEHDR wHead[NBUFFERS];
static wmmCallback WMMCallback = NULL;
static bool volatile running = FALSE;
int prepareError = 0;


WINDOWSSOUNDJNA_API int enumerateDevices() {
	int t = mixerGetNumDevs();
	usedDevices = (int*) calloc(t, sizeof(int));
	deviceCaps = (WAVEINCAPS*) calloc(t, sizeof(WAVEINCAPS));
	nDevices = 0;
	for (int i = 0; i < t; i++) {
		WAVEINCAPS* caps = deviceCaps + nDevices;
		int ret = waveInGetDevCaps(i, caps, sizeof(WAVEINCAPS));
		// copy the name to a simple character array. 
		char name[32];
		for (int j = 0; j < 32; j++) {
			name[j] = (char)caps->szPname[i];
		}
		name[31] = 0;
		if (strlen(name)) {
			nDevices++;
		}
	}
	return nDevices;
}
WINDOWSSOUNDJNA_API int getNumDevices() {
	return nDevices;
}

static char name[32];

WINDOWSSOUNDJNA_API char* getDeviceName(int iDevice) {
	if (iDevice >= nDevices) {
		return NULL;
	}
	WAVEINCAPS* caps = deviceCaps + iDevice;
	//char name[32];
	for (int i = 0; i < 32; i++) {
		name[i] = (char)caps->szPname[i];
	}
	name[31] = 0;
	//memcpy(name, (char*)(caps.szPname), 32);
	//printf(name);
	//printf("\n");
	//fflush(stdout);
	return name;
}


WINDOWSSOUNDJNA_API WCHAR* getDeviceName2(int iDevice) {
	if (iDevice >= nDevices) {
		return NULL;
	}
	WAVEINCAPS* caps = deviceCaps + iDevice;
	return caps->szPname;
}


WINDOWSSOUNDJNA_API int getDeviceFormats(int iDevice) {
	if (iDevice >= nDevices) {
		return -1;
	}
	return deviceCaps[iDevice].dwFormats;
}

WINDOWSSOUNDJNA_API int getDeviceChannels(int iDevice) {
	if (iDevice >= nDevices) {
		return -1;
	}
	return deviceCaps[iDevice].wChannels;
}

void clearBuffers() {
	if (buffers == NULL) {
		return;
	}
	for (int i = 0; i < NBUFFERS; i++) {
		free(buffers[i]);
		free(wHead[i]);
	}
	free(buffers);
	buffers = NULL;
}

void createBuffers(int bufferBytes) {
	clearBuffers();
	buffers = (void**)calloc(sizeof(void*), NBUFFERS);
	bufferLenBytes = bufferBytes;
	for (int i = 0; i < NBUFFERS; i++) {
		buffers[i] = malloc(bufferBytes);
		wHead[i] = (LPWAVEHDR) calloc(sizeof(WAVEHDR), 1);
		wHead[i]->lpData = (char*) buffers[i];
		wHead[i]->dwBytesRecorded = 0;
		wHead[i]->dwUser = i;   // so I know which one it is when its returned !
		wHead[i]->dwFlags = 0;
		wHead[i]->dwBufferLength = bufferLenBytes;
	}
}

// https://learn.microsoft.com/en-us/previous-versions/dd743849(v=vs.85)
void CALLBACK waveInProc(
	HWAVEIN   hwi,
	UINT      uMsg,
	DWORD_PTR dwInstance,
	DWORD_PTR dwParam1,
	DWORD_PTR dwParam2
) {
	LPWAVEHDR thisWaveHdr = (LPWAVEHDR)dwParam1;
	switch (uMsg) {
	case WIM_OPEN:
		//printf("waveInProc OPEN : %d\n", uMsg);
		break;
	case WIM_CLOSE:
		//printf("waveInProc CLOSE : %d\n", uMsg);
		break;
	case WIM_DATA:
		// get the data from the buffer and send it to the jna callback, then recycle the buffer
		//printf("waveInProc DATA : %d block %d\n", uMsg, (int) thisWaveHdr->dwUser);
		//waveInPrepareHeader(hWaveIn, thisWaveHdr, sizeof(WAVEHDR));
		if (running && WMMCallback) {
			WMMCallback((const char*)thisWaveHdr->lpData, bufferLenBytes);
		}
		waveInAddBuffer(hWaveIn, thisWaveHdr, sizeof(WAVEHDR));
		break;
	default:
		//printf("waveInProc other : %d\n", uMsg);
		break;
	}
	//fflush(stdout);
}

WINDOWSSOUNDJNA_API int wavePrepare(int iDevice, int nChannels, int sampleRate, int bitDepth, wmmCallback callBackFn) {
	WMMCallback = callBackFn;
	WAVEFORMATEX waveFormat;
	waveFormat.cbSize = 0;
	waveFormat.nChannels = nChannels;
	waveFormat.nSamplesPerSec = sampleRate;
	waveFormat.wBitsPerSample = bitDepth;
	waveFormat.nBlockAlign = bitDepth / 8 * nChannels;
	waveFormat.nAvgBytesPerSec = waveFormat.nBlockAlign * sampleRate;
	waveFormat.wFormatTag = WAVE_FORMAT_PCM;

	prepareError = waveInOpen(&hWaveIn, iDevice, &waveFormat, (DWORD_PTR) waveInProc, NULL, CALLBACK_FUNCTION);

	if (prepareError != MMSYSERR_NOERROR) {
		hWaveIn = NULL;
		return prepareError;
	}
	bufferLenBytes = waveFormat.nAvgBytesPerSec / 10;
	if (bufferLenBytes < 1024) {
		bufferLenBytes = 1024;
	}
	createBuffers(bufferLenBytes);
	// and send all the buffers to the device. 
	for (int i = 0; i < NBUFFERS; i++) {
		waveInPrepareHeader(hWaveIn, wHead[i], sizeof(WAVEHDR));
		waveInAddBuffer(hWaveIn, wHead[i], sizeof(WAVEHDR));
	}

	
}

WINDOWSSOUNDJNA_API int waveStart() {
	if (hWaveIn == NULL || prepareError != MMSYSERR_NOERROR) {
		return prepareError;
	}
	running = TRUE;
	int res = waveInStart(hWaveIn);
	return res;
}

WINDOWSSOUNDJNA_API int waveStop() {
	running = FALSE;
	if (hWaveIn == NULL) {
		return 0;
	}
	waveInStop(hWaveIn);
	waveInClose(hWaveIn);
	hWaveIn = NULL;
	return 0;
}

/*#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { return 0; }
#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

WINDOWSSOUNDJNA_API int wEnumerateDevices() {
	HRESULT hr = S_OK;
	IMMDeviceEnumerator *pEnumerator = NULL;
	IMMDeviceCollection *pCollection = NULL;
	IMMDevice *pEndpoint = NULL;
	IPropertyStore *pProps = NULL;
	LPWSTR pwszID = NULL;

	hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator, NULL,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		(void**)&pEnumerator);

	EXIT_ON_ERROR(hr)

		hr = pEnumerator->EnumAudioEndpoints(
			eRender, DEVICE_STATE_ACTIVE,
			&pCollection);
	EXIT_ON_ERROR(hr)

		UINT  count;
	hr = pCollection->GetCount(&count);
	EXIT_ON_ERROR(hr)

		if (count == 0)
		{
			printf("No endpoints found.\n");
		}
	// Each loop prints the name of an endpoint device.
	for (ULONG i = 0; i < count; i++)
	{
		// Get pointer to endpoint number i.
		hr = pCollection->Item(i, &pEndpoint);
		EXIT_ON_ERROR(hr)

			// Get the endpoint ID string.
			hr = pEndpoint->GetId(&pwszID);
		EXIT_ON_ERROR(hr)

			hr = pEndpoint->OpenPropertyStore(
				STGM_READ, &pProps);
		EXIT_ON_ERROR(hr)

			PROPVARIANT varName;
		// Initialize container for property value.
		PropVariantInit(&varName);

		// Get the endpoint's friendly-name property.
		hr = pProps->GetValue(
			PKEY_Device_FriendlyName, &varName);
		EXIT_ON_ERROR(hr)

			// Print endpoint friendly name and endpoint ID.
			printf("Endpoint %d: \"%S\" (%S)\n",
				i, varName.pwszVal, pwszID);

		CoTaskMemFree(pwszID);
		pwszID = NULL;
		PropVariantClear(&varName);
		SAFE_RELEASE(pProps)
			SAFE_RELEASE(pEndpoint)
	}
	SAFE_RELEASE(pEnumerator)
		SAFE_RELEASE(pCollection)
		return;
}*/