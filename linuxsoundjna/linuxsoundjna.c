/*
 * linuxsoundjna.c
 * 
 * Copyright 2025  <dg50@raspberrypi>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * Build command in Geany set to gcc -shared -o linuxsound.so "%f" -lasound
 * Otherwise seems OK with default build parameters. 
 */


#include <stdio.h>
#include <threads.h>
#include <alsa/asoundlib.h>

/*
 * Definition of the callback function to the Java side
 */
typedef int(*wmmCallback)(const char* data, int dataLen);
// declaration of said callback function
static wmmCallback WMMCallback = NULL;

static int nDevices = 0;
static char** deviceNames = NULL;
static char** longNames = NULL;
#define MAX_DEVICES 10
static char* readBuffer = NULL;
static int readBufferLength = 0;
static int readBufferFrames = 0;

static snd_pcm_t *capture_handle;
static snd_pcm_hw_params_t *hw_params;
static thrd_t readThread;

static int pcmThread(void* thread_data) {
	int err;
	int frames;
	while (capture_handle != NULL) {
		frames = snd_pcm_readi(capture_handle, readBuffer, readBufferFrames);
		WMMCallback(readBuffer, readBufferLength);
	}
}

int enumerateDevices() {
    int cardNum = -1;     // Start with first card
    int err;
    nDevices = 0;
    if (deviceNames == NULL) {
		deviceNames = (char**) malloc(MAX_DEVICES * sizeof(char*));
		longNames = (char**) malloc(MAX_DEVICES * sizeof(char*));
	}

    for (;;) {
        // Get next sound card's card number.
        if ((err = snd_card_next(&cardNum)) < 0) {
            fprintf(stderr, "Can't get the next card number: %s\n",
                            snd_strerror(err));
            break;
        }

        if (cardNum < 0) {
            // No more cards
            break;
		}
		if (deviceNames[nDevices] == NULL) {
			deviceNames[nDevices] = (char*) malloc(256*sizeof(char));
			longNames[nDevices] = (char*) malloc(256*sizeof(char));
		}
		// get the sound card name. 
		snd_card_get_name(cardNum, &deviceNames[nDevices]);
		deviceNames[nDevices] = realloc(deviceNames[nDevices], strlen(deviceNames[nDevices])+1);
		snd_card_get_longname(cardNum, &longNames[nDevices]);
		fprintf(stderr, "Card %d Name %d \"%s\" long name \"%s\"\n", nDevices,strlen(deviceNames[nDevices]), 
			deviceNames[nDevices], longNames[nDevices]);

        ++nDevices;   // Another card found, so bump the count
    }
    return nDevices;
}

int getNumDevices() {
	return nDevices;
}

char* getDeviceName(int iDevice) {
	return deviceNames[iDevice];
}

char* getDeviceName2(int iDevice) {
	char* name = deviceNames[iDevice];
	int len = strlen(name);
	char* tmp = malloc(len+1);
	memcpy(tmp, name, len);
	tmp[len] = 0;
	return tmp;
}

int getDeviceFormats(int iDevice) {
	return 0xFF;
}

int getDeviceChannels(int iDevice) {
	return 2;
}
int wavePrepare(int iDevice, int nChannels, int sampleRate, int bitDepth, wmmCallback callBackFn) {
	// example at https://gist.github.com/albanpeignier/104902
	WMMCallback = callBackFn;
	int err;
	snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
	if (bitDepth == 24) {
		format = SND_PCM_FORMAT_S24_LE;
	}
	char* devName = getDeviceName(iDevice);
	char name[20];
	sprintf(name, "hw:%d",iDevice);
	if ((err = snd_pcm_open(&capture_handle, name, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		printf("Unable to open sound device \"%s\": %s\n", name, snd_strerror(err));
		return err;
	}
	if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
		fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n", 
             snd_strerror (err));
		return err;
	}
	if ((err = snd_pcm_hw_params_any (capture_handle, hw_params)) < 0) {
		fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n",
             snd_strerror (err));
		return err;
	}
	if ((err = snd_pcm_hw_params_set_access (capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		fprintf (stderr, "cannot set access type (%s)\n",
             snd_strerror (err));
		return err;
	}
		
	if ((err = snd_pcm_hw_params_set_format (capture_handle, hw_params, format)) < 0) {
		fprintf (stderr, "cannot set sample format (%s)\n",
             snd_strerror (err));
		return err;
	}	
		
	unsigned int rate = sampleRate;
	if ((err = snd_pcm_hw_params_set_rate_near (capture_handle, hw_params, &rate, 0)) < 0) {
		fprintf (stderr, "cannot set sample rate (%s)\n",
             snd_strerror (err));
		return err;
	}
		
	if ((err = snd_pcm_hw_params_set_channels (capture_handle, hw_params, nChannels)) < 0) {
		fprintf (stderr, "cannot set channel count (%s)\n",
		snd_strerror (err));
		return err;
	}
	
	if ((err = snd_pcm_hw_params (capture_handle, hw_params)) < 0) {
		fprintf (stderr, "cannot set parameters (%s)\n",
			snd_strerror (err));
		return err;
	}
	
	if (readBuffer != NULL) {
		free(readBuffer);
	}
	readBufferFrames = sampleRate / 10;
	readBufferLength = readBufferFrames * nChannels * snd_pcm_format_width(format) / 8;	
	readBuffer = (char*) malloc(readBufferLength);	
		
	return 0;
}

int waveStart() {
	// start a read thread. 
	thrd_create(&readThread, pcmThread, NULL);
	
	int err;
    if ((err = snd_pcm_prepare (capture_handle)) < 0) {
		fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
			snd_strerror (err));
		return err;
	}
	return 0;
}

int waveStop() {
	if (capture_handle != NULL) {
		snd_pcm_close (capture_handle);
		capture_handle = NULL;
		if (readThread != 0) {
			thrd_join(readThread, NULL);
		}
		readThread = 0;
		free(readBuffer);
		readBuffer = NULL;
	}
	return 0;
}
	

