# Linux: audioClips play through PulseAudio / PipeWire

The Linux engine played audioClips through EsounD (esd), a sound daemon
that desktop distributions stopped shipping more than a decade ago, so
`play audioClip` produced no sound on current Linux systems. The esd code
has been replaced with PulseAudio's simple API, which every current
desktop provides, either from PulseAudio itself or from PipeWire's
pipewire-pulse service.

The library (libpulse-simple.so.0) is loaded at runtime, so nothing extra
is needed to build the engine. On a system without it audioClips stay
silent, as before, and `playStopped` is still sent.

* `play audioClip` plays 8-bit, 16-bit and 24-bit clips, mono or stereo,
  imported from AU, AIFF and WAV files, with or without `looping`.
* `play stop` stops the clip promptly, and `the sound` returns "done" and
  the `playStopped` message is sent once the clip finishes.
* `the playLoudness`, and the `playLoudness` property of an audioClip,
  now scale the output on Linux.
