# Windows

Windows DLL to work with [https://github.com/douggillespie/WindowsSound](https://github.com/douggillespie/WindowsSound)
which is simply a wrapper around some low level Windows multimedia function, e.g. waveInStart() to acquire sound card 
data into PAMGuard without the restrictions of Javasound. 

# Linux

Linux shared library to use Linux ALSA library functions to control the sound card. 

You should build the c file linuxsoundjna/linuxsoundjna.c for your linux version using gcc:

```
gcc -shared -o linuxsound.so linuxsoundjna.c -lasound
```

then copy the shared libary linuxsound.so to your java library path.